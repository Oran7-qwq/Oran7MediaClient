#include "Oran7VideoThumbnailCache.h"
#include "Oran7Async.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QColorSpace>
#include <QtEndian>
#include <algorithm>
#include <array>
#include <cmath>

namespace
{
constexpr int width = 320, height = 180;
QString fingerprint(const QString& path)
{
    const QFileInfo file(path);
    if (!file.isFile() || !file.isReadable())
        return {};
    const auto value = QStringLiteral("v2-hdr-srgb|%1|%2|%3")
                           .arg(file.absoluteFilePath().toCaseFolded())
                           .arg(file.size())
                           .arg(file.lastModified().toMSecsSinceEpoch())
                           .toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(value, QCryptographicHash::Sha256).toHex());
}
struct Oran7ThumbnailColor
{
    int transfer = 0;
    bool bt2020 = false;
    bool fullRange = false;
    QString matrix;
    double peakNits = 1000;
};
bool runHelper(const QString& executable, const QStringList& arguments, QByteArray& output,
               const std::shared_ptr<std::atomic_bool>& stopped)
{
    if (stopped->load())
        return false;
    QProcess process;
    process.start(executable, arguments);
    QElapsedTimer timer;
    timer.start();
    while (!process.waitForFinished(100))
    {
        if (stopped->load() || timer.elapsed() > 5000 || process.error() == QProcess::FailedToStart)
        {
            process.kill();
            process.waitForFinished(1000);
            return false;
        }
    }
    if (stopped->load() || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return false;
    output = process.readAllStandardOutput();
    return true;
}
bool probeColor(const QString& executable, const QString& path, Oran7ThumbnailColor& color,
                const std::shared_ptr<std::atomic_bool>& stopped)
{
    QByteArray json;
    const auto probe = QFileInfo(executable).dir().absoluteFilePath("ffprobe.exe");
    if (!runHelper(probe,
                   {"-v", "error", "-protocol_whitelist", "file,pipe", "-select_streams", "V:0", "-show_entries",
                    "stream=color_space,color_transfer,color_primaries,color_range:stream_side_data", "-of", "json",
                    path},
                   json, stopped))
        return false;
    const auto streams = QJsonDocument::fromJson(json).object().value("streams").toArray();
    if (streams.isEmpty())
        return false;
    const auto stream = streams.first().toObject();
    const auto transfer = stream.value("color_transfer").toString();
    color.transfer = transfer == "smpte2084" ? 1 : transfer == "arib-std-b67" ? 2 : 0;
    color.matrix = stream.value("color_space").toString();
    color.bt2020 = stream.value("color_primaries").toString() == "bt2020" || color.matrix == "bt2020nc";
    color.fullRange = stream.value("color_range").toString() == "pc";
    for (const auto& value : stream.value("side_data_list").toArray())
    {
        const auto metadata = value.toObject();
        const auto rational = metadata.value("max_luminance").toString().split('/');
        if (rational.size() == 2 && rational[1].toDouble() > 0)
        {
            const double peak = rational[0].toDouble() / rational[1].toDouble();
            if (peak >= 100 && peak <= 10000)
                color.peakNits = peak;
        }
    }
    for (const auto& value : stream.value("side_data_list").toArray())
    {
        const double peak = value.toObject().value("max_content").toDouble();
        if (peak >= 100 && peak <= 10000)
            color.peakNits = peak;
    }
    return true;
}
std::array<double, 3> mapHdr(std::array<double, 3> rgb, const Oran7ThumbnailColor& color)
{
    // 与 shaders/video.frag 的 SDR 输出保持一致：绝对 nit、线性色域、亮度映射、sRGB。
    for (auto& channel : rgb)
    {
        if (color.transfer == 1)
        {
            const auto p = std::pow(channel, 1.0 / 78.84375);
            channel = 10000 * std::pow(std::max(p - 0.8359375, 0.0) / std::max(18.8515625 - 18.6875 * p, 1e-6),
                                       1.0 / 0.1593017578125);
        }
        else
        {
            channel = channel <= 0.5 ? channel * channel / 3
                                     : (std::exp((channel - 0.55991073) / 0.17883277) + 0.28466892) / 12;
        }
    }
    if (color.transfer == 2)
    {
        const auto luminance = color.bt2020 ? rgb[0] * 0.2627 + rgb[1] * 0.6780 + rgb[2] * 0.0593
                                            : rgb[0] * 0.2126 + rgb[1] * 0.7152 + rgb[2] * 0.0722;
        const double gain = std::pow(std::max(luminance, 1e-6), 0.2) * 1000;
        for (auto& channel : rgb)
            channel *= gain;
    }
    if (color.bt2020)
    {
        const auto input = rgb;
        rgb = {1.660491 * input[0] - 0.587641 * input[1] - 0.072850 * input[2],
               -0.124550 * input[0] + 1.132900 * input[1] - 0.008349 * input[2],
               -0.018151 * input[0] - 0.100579 * input[1] + 1.118730 * input[2]};
    }
    const double luminance = std::max(0.0, rgb[0] * 0.2126 + rgb[1] * 0.7152 + rgb[2] * 0.0722);
    const double x = luminance / 203;
    const double white = std::max(1.0, color.peakNits / 203);
    const double y = std::clamp(x * (1 + x / (white * white)) / (1 + x), 0.0, 1.0);
    double saturation = 1;
    for (auto& channel : rgb)
    {
        channel = channel * y / std::max(luminance, 1e-6) - y;
        if (channel > 1e-6)
            saturation = std::min(saturation, (1 - y) / channel);
        if (channel < -1e-6)
            saturation = std::min(saturation, -y / channel);
    }
    for (auto& channel : rgb)
    {
        channel = std::clamp(y + saturation * channel, 0.0, 1.0);
        channel = channel <= 0.0031308 ? channel * 12.92 : 1.055 * std::pow(channel, 1.0 / 2.4) - 0.055;
    }
    return rgb;
}
QImage extract(const QString& executable, const QString& path, int seconds, const Oran7ThumbnailColor& color,
               const std::shared_ptr<std::atomic_bool>& stopped)
{
    const bool hdr = color.transfer != 0;
    QString filter = "scale=320:180:force_original_aspect_ratio=decrease";
    if (hdr)
    {
        if (color.matrix == "bt2020nc" || color.bt2020)
            filter += ":in_color_matrix=bt2020";
        else if (color.matrix == "bt709")
            filter += ":in_color_matrix=bt709";
        filter += color.fullRange ? ":in_range=pc:out_range=pc" : ":in_range=tv:out_range=pc";
        filter += ",format=rgb48le";
    }
    filter += ",pad=320:180:(ow-iw)/2:(oh-ih)/2";
    QByteArray pixels;
    if (!runHelper(executable,
                   {"-nostdin",
                    "-hide_banner",
                    "-loglevel",
                    "error",
                    "-threads",
                    "1",
                    "-protocol_whitelist",
                    "file,pipe",
                    "-ss",
                    QString::number(seconds),
                    "-i",
                    path,
                    "-map",
                    "0:V:0",
                    "-an",
                    "-sn",
                    "-dn",
                    "-frames:v",
                    "1",
                    "-vf",
                    filter,
                    "-threads",
                    "1",
                    "-pix_fmt",
                    hdr ? "rgb48le" : "rgb24",
                    "-f",
                    "rawvideo",
                    "pipe:1"},
                   pixels, stopped))
        return {};
    if (pixels.size() != width * height * (hdr ? 6 : 3))
        return {};
    if (!hdr)
    {
        auto image =
            QImage(reinterpret_cast<const uchar*>(pixels.constData()), width, height, width * 3, QImage::Format_RGB888)
                .copy();
        image.setColorSpace(QColorSpace::SRgb);
        return image;
    }
    QImage image(width, height, QImage::Format_RGB888);
    const auto* data = reinterpret_cast<const uchar*>(pixels.constData());
    for (int row = 0; row < height; ++row)
    {
        if (stopped->load())
            return {};
        auto* line = image.scanLine(row);
        for (int col = 0; col < width; ++col)
        {
            const auto* pixel = data + (row * width + col) * 6;
            const auto rgb =
                mapHdr({qFromLittleEndian<quint16>(pixel) / 65535.0, qFromLittleEndian<quint16>(pixel + 2) / 65535.0,
                        qFromLittleEndian<quint16>(pixel + 4) / 65535.0},
                       color);
            for (int channel = 0; channel < 3; ++channel)
                line[col * 3 + channel] = uchar(std::lround(rgb[channel] * 255));
        }
    }
    image.setColorSpace(QColorSpace::SRgb);
    return image;
}
void trimCache(const QString& directory, const QString& keep)
{
    const auto files = QDir(directory).entryInfoList({"*.jpg"}, QDir::Files, QDir::Time | QDir::Reversed);
    qint64 total = 0;
    for (const auto& file : files)
        total += file.size();
    for (const auto& file : files)
    {
        if (total <= 256LL * 1024 * 1024)
            break;
        if (file.absoluteFilePath() != keep && QFile::remove(file.absoluteFilePath()))
            total -= file.size();
    }
}
QString thumbnail(const QString& path, const QString& directory, const QString& executable,
                  const std::shared_ptr<std::atomic_bool>& stopped)
{
    const auto key = fingerprint(path);
    if (key.isEmpty() || stopped->load())
        return {};
    const auto output = QDir(directory).absoluteFilePath(key + ".jpg");
    const QFileInfo cached(output);
    if (cached.isFile() && cached.size() < 1024 * 1024)
    {
        QImage image(output);
        if (image.size() == QSize(width, height))
        {
            QFile touch(output);
            if (touch.open(QIODevice::ReadWrite))
                touch.setFileTime(QDateTime::currentDateTimeUtc(), QFileDevice::FileModificationTime);
            return QUrl::fromLocalFile(output).toString();
        }
    }
    Oran7ThumbnailColor color;
    // 探测失败保留占位图，不能将未知的 PQ/HLG 按普通 RGB 错误生成持久缓存。
    if (!probeColor(executable, path, color, stopped))
        return {};
    auto image = extract(executable, path, 2, color, stopped);
    if (image.isNull() && !stopped->load())
        image = extract(executable, path, 0, color, stopped);
    if (image.isNull() || stopped->load() || fingerprint(path) != key || !QDir().mkpath(directory))
        return {};
    QSaveFile file(output);
    if (!file.open(QIODevice::WriteOnly) || !image.save(&file, "JPG", 85) || !file.commit())
        return {};
    trimCache(directory, output);
    return QUrl::fromLocalFile(output).toString();
}
} // namespace

Oran7VideoThumbnailCache::Oran7VideoThumbnailCache(QString directory, QString executable)
    : m_directory(directory.isEmpty()
                      ? QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/video-thumbnails"
                      : directory),
      m_executable(executable.isEmpty() ? QCoreApplication::applicationDirPath() + "/ffmpeg.exe" : executable),
      m_stopped(std::make_shared<std::atomic_bool>(false))
{
}
Oran7VideoThumbnailCache::~Oran7VideoThumbnailCache()
{
    shutdown();
}
void Oran7VideoThumbnailCache::shutdown()
{
    m_stopped->store(true);
    m_queue.clear();
}
void Oran7VideoThumbnailCache::request(const QString& path)
{
    if (m_stopped->load() || m_pending.contains(path) || m_queue.size() >= 50)
        return;
    m_pending.insert(path);
    m_queue.enqueue(path);
    startNext();
}
void Oran7VideoThumbnailCache::startNext()
{
    if (m_running || m_queue.isEmpty() || Oran7Async::isShuttingDown())
        return;
    const auto path = m_queue.dequeue();
    m_running = true;
    Oran7Async::run(
        this,
        [path, directory = m_directory, executable = m_executable, stopped = m_stopped]
        { return thumbnail(path, directory, executable, stopped); },
        [this, path](const QString& url)
        {
            m_pending.remove(path);
            m_running = false;
            if (m_stopped->load())
                return;
            emit ready(path, url);
            startNext();
        });
}
