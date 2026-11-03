#include "Oran7RenderBridge.h"
#include <QGuiApplication>
#include <QCoreApplication>
#include <thread>
#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <QFile>
static std::atomic_int freed{0};
static void release(void*, uint8_t* ptr) { ++freed; av_free(ptr); }
static AVFrame* frame(int width) {
 auto* f=av_frame_alloc(); f->width=width;
 f->buf[0]=av_buffer_create(static_cast<uint8_t*>(av_malloc(1)),1,release,nullptr,0); return f;
}
static void check(bool ok) { if(!ok) { qFatal("render bridge regression failed"); } }
int main(int argc,char** argv) {
 QGuiApplication app(argc,argv);
 using Bridge=Oran7RenderBridge;
 using Key=Bridge::RenderObject;
 auto* bridge=new Bridge;
 check(!bridge->attachVideoItem(Key::VideoPlayerRender,nullptr));
 check(!bridge->renderBlackFrame(Key::VideoPlayerRender));
 QQuickItem host; host.setSize({800,600});
 check(bridge->attachVideoItem(Key::VideoPlayerRender,&host));
 auto* item=static_cast<D3D11VideoItem*>(bridge->getVideoItem(Key::VideoPlayerRender));
 QVariantMap reported;
 QObject::connect(bridge,&Bridge::videoRenderInfoUpdated,bridge,[&](const QVariantMap& map){reported=map;});
 std::thread infoProducer([&]{ Oran7VideoInfo info; emit item->sendVideoFrameInfo(info); });
 infoProducer.join(); QCoreApplication::processEvents();
 check(reported.value("sourceTag").toString()=="player");
 bridge->setVideoSourceSize(Key::VideoPlayerRender,{1920,1080});
 QCoreApplication::processEvents();
 check(item->rect==QRectF(0,75,800,450));
 bridge->setScaleMode(Key::VideoPlayerRender,Bridge::ScaleMode::Fill);
 check(item->rect==QRectF(0,0,800,600));
 auto first=bridge->beginPlayback();
 std::thread producer([&]{ for(int i=1;i<=500;++i) first(nullptr,frame(i)); }); producer.join();
 check(freed==499); check(item->received==0);
 QCoreApplication::processEvents(); check(item->received==1 && item->lastWidth==500); check(freed==500);
 first(nullptr,frame(501)); bridge->stopPlayback(); first(nullptr,frame(502));
 QCoreApplication::processEvents(); check(item->received==1 && freed==502);
 auto second=bridge->beginPlayback(); first(nullptr,frame(503)); second(nullptr,frame(504));
 QCoreApplication::processEvents(); check(item->received==2 && item->lastWidth==504 && freed==504);
 QQuickItem other; other.setSize({300,200});
 bridge->attachVideoItem(Key::VideoPlayerRender,&other);
 check(item->parentItem()==&other && item->parent()==bridge);
 host.setWidth(999); other.setWidth(350); QCoreApplication::processEvents(); check(item->width()==350);
 auto* transient=new QQuickItem;
 transient->setSize({100,100}); bridge->attachVideoItem(Key::VideoPlayerRender,transient);
 QPointer<D3D11VideoItem> alive(item); delete transient;
 check(alive && !item->parentItem());
 check(bridge->attachVideoItem(Key::VideoPlayerRender,&other));
 // 析构与生产线程同时执行；receiver 失效必须与排队投递串行化。
 std::atomic_bool producing{false};
 std::thread concurrent([&]{ producing=true; for(int i=0;i<10000;++i) second(nullptr,frame(i)); });
 while(!producing.load()) std::this_thread::yield();
 // 销毁桥之后，仍在后台退出的旧播放器回调只能释放帧。
 second(nullptr,frame(505)); delete bridge; concurrent.join();
 std::thread retired([&]{ for(int i=0;i<1000;++i) second(nullptr,frame(i)); }); retired.join();
 QCoreApplication::processEvents(); check(freed==11505);
 QFile proof("tests/render-bridge/result.txt"); check(proof.open(QIODevice::WriteOnly | QIODevice::Truncate)); proof.write("PASS: GUI layout, bounded frame handoff, stop/switch generations, reparenting, retired callback lifetime, frame release\n");
 std::puts("PASS: render bridge regression checks");
}
