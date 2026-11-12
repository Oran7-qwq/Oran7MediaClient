#include "Oran7VideoThumbnailCache.h"
#include "Oran7Async.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QProcess>
#include <QSignalSpy>
#include <QElapsedTimer>
#include <QImage>
#include <QFile>
#include <QUrl>
#include <QDateTime>
#include <QTest>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QtEndian>
#include <cmath>
#include <array>
#include <cstdio>
#include <cstdlib>

static void check(bool ok, const char* message)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
static QString request(Oran7VideoThumbnailCache& cache, const QString& source)
{
    QSignalSpy spy(&cache, &Oran7VideoThumbnailCache::ready);
    cache.request(source);
    QElapsedTimer timer;
    timer.start();
    while (spy.isEmpty() && timer.elapsed() < 12000)
        QTest::qWait(20);
    check(spy.size() == 1, "thumbnail request completes");
    return spy.first().at(1).toString();
}
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    const auto executable = QStringLiteral(APP_ROOT) + "/ffmpeg6.0/bin/ffmpeg.exe";
    const auto video = dir.filePath("短视频 sample.mkv");
    const auto directory = dir.filePath("cache");
    QProcess generate;
    generate.start(executable, {"-nostdin", "-hide_banner", "-loglevel", "error", "-f", "lavfi", "-i",
                                "color=c=red:size=160x90:rate=5", "-t", "1", "-c:v", "ffv1", "-y", video});
    check(generate.waitForFinished(10000) && generate.exitCode() == 0,
          "generate synthetic short video without user files");
    QDir().mkpath(directory);
    const QFileInfo sourceInfo(video);
    const auto legacyFingerprint = QStringLiteral("v1|%1|%2|%3")
                                       .arg(sourceInfo.absoluteFilePath().toCaseFolded())
                                       .arg(sourceInfo.size())
                                       .arg(sourceInfo.lastModified().toMSecsSinceEpoch())
                                       .toUtf8();
    const auto legacy =
        directory + "/" +
        QString::fromLatin1(QCryptographicHash::hash(legacyFingerprint, QCryptographicHash::Sha256).toHex()) + ".jpg";
    QImage legacyImage(320, 180, QImage::Format_RGB32);
    legacyImage.fill(Qt::blue);
    check(legacyImage.save(legacy), "seed old thumbnail algorithm cache");
    Oran7VideoThumbnailCache cache(directory, executable);
    const auto first = request(cache, video);
    const auto fileName = QUrl(first).toLocalFile();
    check(fileName != legacy, "new color pipeline invalidates pre-HDR thumbnail caches without modifying the source");
    QImage image(fileName);
    check(image.size() == QSize(320, 180) && image.pixelColor(160, 90).red() > 150,
          "short video yields an actual decoded frame with first-frame fallback");
    Oran7VideoThumbnailCache restarted(directory, dir.filePath("unavailable-ffmpeg.exe"));
    check(request(restarted, video) == first, "new cache instance uses disk thumbnail without an extractor");
    {
        QFile corrupt(fileName);
        check(corrupt.open(QIODevice::WriteOnly | QIODevice::Truncate), "open synthetic corrupt cache");
        corrupt.write("invalid image");
    }
    check(request(cache, video) == first && !QImage(fileName).isNull(), "corrupt cache is regenerated");
    {
        QFile changed(video);
        check(changed.open(QIODevice::ReadWrite), "open synthetic source");
        check(changed.setFileTime(QDateTime::currentDateTimeUtc().addSecs(10), QFileDevice::FileModificationTime),
              "change source fingerprint");
    }
    const auto second = request(cache, video);
    check(!second.isEmpty() && second != first, "file modification invalidates its cached thumbnail");
    check(request(cache, dir.filePath("missing.mkv")).isEmpty(), "missing input produces a placeholder result");
    check(QFile::remove(video) && request(cache, video).isEmpty(), "removed source cannot reuse a stale disk image");
    const auto raw = dir.filePath("hdr.rgb");
    const auto pq = [](double nits)
    {
        const double y = std::pow(nits / 10000.0, 0.1593017578125);
        return std::pow((0.8359375 + 18.8515625 * y) / (1 + 18.6875 * y), 78.84375);
    };
    const auto createHdr = [&](const QString& name, const QString& transfer)
    {
        QByteArray pixels(160 * 90 * 6, char(0));
        for (int y = 0; y < 90; ++y)
            for (int x = 0; x < 160; ++x)
            {
                std::array<double, 3> color{};
                if (x < 40)
                    color.fill(transfer == "smpte2084" ? pq(100) : 0.25);
                else if (x < 80)
                    color = {transfer == "smpte2084" ? pq(100) : 0.75, 0, 0};
                else if (x < 120)
                    color.fill(transfer == "smpte2084" ? pq(1000) : 1);
                for (int c = 0; c < 3; ++c)
                    qToLittleEndian<quint16>(quint16(std::lround(color[c] * 65535)),
                                             reinterpret_cast<uchar*>(pixels.data()) + (y * 160 + x) * 6 + c * 2);
            }
        QFile file(raw);
        check(file.open(QIODevice::WriteOnly), "write high precision fixture");
        check(file.write(pixels) == pixels.size(), "write complete RGB48 frame");
        file.close();
        const auto output = dir.filePath(name);
        QProcess encoder;
        encoder.start(executable, {"-nostdin",
                                   "-hide_banner",
                                   "-loglevel",
                                   "error",
                                   "-f",
                                   "rawvideo",
                                   "-pixel_format",
                                   "rgb48le",
                                   "-video_size",
                                   "160x90",
                                   "-framerate",
                                   "1",
                                   "-i",
                                   raw,
                                   "-frames:v",
                                   "1",
                                   "-c:v",
                                   "ffv1",
                                   "-pix_fmt",
                                   "gbrp16le",
                                   "-color_primaries",
                                   "bt2020",
                                   "-color_trc",
                                   transfer,
                                   "-colorspace",
                                   "bt2020nc",
                                   "-color_range",
                                   "pc",
                                   "-y",
                                   output});
        check(encoder.waitForFinished(10000) && encoder.exitCode() == 0, "encode tagged high bit depth HDR fixture");
        return output;
    };
    const auto pqVideo = createHdr("PQ.mkv", "smpte2084");
    QImage pqImage(QUrl(request(cache, pqVideo)).toLocalFile());
    check(!pqImage.isNull(), "PQ thumbnail generated with high precision extraction");
    const auto gray = pqImage.pixelColor(40, 90), red = pqImage.pixelColor(120, 90);
    check(gray.red() > 145 && gray.red() < 170 && std::abs(gray.red() - gray.green()) < 4,
          "100 nit PQ neutral is tone mapped instead of stored as encoded gamma");
    check(red.red() > 180 && red.green() < 20 && red.blue() < 110,
          "BT2020 red uses linear gamut mapping rather than losing saturation");
    check(pqImage.pixelColor(200, 90).red() > 248 && pqImage.pixelColor(280, 90).red() < 5,
          "PQ highlight and black survive JPEG conversion");
    pqImage.save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR) + "/pq-thumbnail.png");
    const auto hlgVideo = createHdr("HLG.mkv", "arib-std-b67");
    QImage hlgImage(QUrl(request(cache, hlgVideo)).toLocalFile());
    check(!hlgImage.isNull() && hlgImage.pixelColor(40, 90).red() > 50 && hlgImage.pixelColor(40, 90).red() < 70,
          "HLG inverse curve and reference display OOTF are applied");
    check(hlgImage.pixelColor(200, 90).red() > 248 && hlgImage.pixelColor(280, 90).red() < 5,
          "HLG highlight and black are correctly mapped");
    hlgImage.save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR) + "/hlg-thumbnail.png");
    check(request(restarted, pqVideo) == QUrl::fromLocalFile(QUrl(request(cache, pqVideo)).toLocalFile()).toString(),
          "HDR cache is reusable without re-extracting");
    if (app.arguments().size() > 1)
    {
        QImage sample(QUrl(request(cache, app.arguments().at(1))).toLocalFile());
        check(!sample.isNull(), "provided HDR source decodes and generates a thumbnail");
        sample.save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR) + "/hdr-source-thumbnail.png");
    }
    cache.shutdown();
    restarted.shutdown();
    Oran7Async::waitForDone();
    std::puts("PASS: actual video frames, durable thumbnail cache and source invalidation");
}
