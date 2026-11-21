#include "Oran7PointerParallax.h"
#include "Oran7ProcessLoadMonitor.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlPropertyMap>
#include <QQuickWindow>
#include <QQuickItem>
#include <QElapsedTimer>
#include <QThread>
#include <QJSValue>
#include <QFontDatabase>
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <cmath>
#include <cstdio>
#include <cstdlib>

static int checks = 0;
static void check(bool ok, const char* message) { if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); } ++checks; }
static void settle() { QElapsedTimer t; t.start(); while(t.elapsed() < 150) { QCoreApplication::processEvents(); QThread::msleep(2); } }
static QQuickItem* findItem(QQuickItem* item, const QString& name) {
    if (item->objectName() == name) return item;
    for (auto* child : item->childItems()) if (auto* found = findItem(child, name)) return found;
    return nullptr;
}

static void checkViewportEditing(QQuickWindow* window, QQuickItem* preview, const QList<QQuickItem*>& outsidePanels)
{
    auto* editor = findItem(preview, "recordingViewportEditor");
    auto* viewport = findItem(preview, "recordingViewport");
    auto* selection = findItem(preview, "recordingViewportSelection");
    auto* encodingCanvas = findItem(preview, "recordingEncodingCanvas");
    auto* loader = findItem(preview, "recordingViewportLoader");
    auto* canvas = findItem(preview, "recordingPreviewCanvas");
    auto* header = findItem(preview, "recordingPreviewHeader");
    auto* selectionTools = findItem(preview, "recordingSelectionTools");
    check(editor && viewport && selection && encodingCanvas && loader && canvas && header && selectionTools,
          "viewport and preview tools are mounted");
    const bool violetSkin = preview->property("violetSkin").toBool();
    auto* loadedPreview = loader->property("item").value<QObject*>();
    check(loadedPreview && !selection->isVisible() && encodingCanvas->property("radius").toReal() == 0,
          "preview starts unselected with a squared encoder canvas");
    const auto* selectionBorder = selection->property("border").value<QObject*>();
    check(selectionBorder && selectionBorder->property("color").value<QColor>() ==
              (violetSkin ? QColor("#b8a7f7") : preview->property("accentColor").value<QColor>()),
          "selection outline follows the skin palette");
    const auto geometry = [&] { return QRectF(viewport->position(), viewport->size()); };
    const auto center = [&](QQuickItem* item) {
        return item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
    };
    const auto tool = [&](const QString& name) { return findItem(selectionTools, "recordingSelection_" + name); };
    const auto click = [&](QQuickItem* button) {
        check(button && button->isVisible() && button->isEnabled(), "preview tool is available");
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(button));
        QCoreApplication::processEvents();
    };
    const auto drag = [&](QPoint start, QPointF delta) {
        const auto end = start + delta.toPoint();
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(window, end, 15);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, end);
        QCoreApplication::processEvents();
    };
    const auto near = [](qreal a, qreal b) { return qAbs(a - b) < 1.5; };
    const auto encodedCanvas = [&] { return editor->property("outputRect").toRectF(); };
    const auto stage = encodedCanvas();
    const auto fitInitial = geometry();
    auto initial = fitInitial;
    const auto aspectMatches = [](const QRectF& rect) { return qAbs(rect.width() / rect.height() - 16.0 / 9) < 0.001; };
    check(aspectMatches(fitInitial) && near(fitInitial.width(), stage.width()) && fitInitial.y() > 0,
          "Fit selection starts at the aspect-correct frame, with letterbox space outside it");
    check(editor->position() == QPointF(0, 0) && editor->size() == canvas->size(),
          "snap edges coincide with all four edges of the visible preview stage");
    check(near(canvas->mapToItem(preview, QPointF(0, canvas->height())).y(), preview->height() - 1),
          "preview canvas extends to the bottom of the panel with only its border");
    QTest::mouseMove(window, center(viewport));
    QCoreApplication::processEvents();
    check(!findItem(preview, "recordingPreviewTools") && !selectionTools->isVisible(),
          "hovering alone reveals no editing toolbar");
    const auto reset = [&] {
        QTest::mouseDClick(window, Qt::LeftButton, Qt::NoModifier, center(viewport));
        QCoreApplication::processEvents();
        check(geometry() == initial, "double click restores the initial viewport");
    };

    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(viewport));
    check(selection->isVisible() && selection->property("radius").toReal() == 0,
          "viewport click shows a square selection outline");
    check(selectionTools->isVisible() && selectionTools->y() + selectionTools->height() <
              viewport->mapToItem(preview, QPointF()).y(), "selected toolbar floats above the preview");
    int previousX = -1;
    for (const auto* name : {"crop", "rotateLeft", "mirror", "fit", "fill", "toFront", "toBack", "reset", "remove"}) {
        auto* button = tool(name);
        check(button && center(button).x() > previousX, "selection actions follow the requested order");
        check(button->width() >= 42 && button->height() >= 40 && button->property("iconSize").toInt() >= 20,
              "selection actions have larger targets and clearly sized icons");
        previousX = center(button).x();
    }
    check(!tool("toFront")->isEnabled() && !tool("toBack")->isEnabled(), "a single layer has no reorder action");
    auto* bottomRight = findItem(preview, "recordingViewportHandle_bottomRight");
    drag(center(bottomRight), QPointF(-50, -46));
    const auto smaller = geometry();
    drag(center(viewport), stage.center() - smaller.center());
    const auto centered = geometry();
    const auto pixels = editor->property("encodingSize").toSize();
    const auto checkGap = [&](const QString& side, qreal length, qreal extent, int resolution) {
        const auto* label = findItem(preview, "recording" + side + "Gap");
        const int expected = 2 * int(std::round(length / extent * resolution / 2));
        check(label && label->isVisible() && expected > 0 &&
                  label->property("text").toString() == QString::number(expected) + " px",
              "selected layer displays the gap in encoder pixels");
    };
    checkGap("Left", centered.left() - stage.left(), stage.width(), pixels.width());
    checkGap("Top", centered.top() - stage.top(), stage.height(), pixels.height());
    checkGap("Right", stage.right() - centered.right(), stage.width(), pixels.width());
    checkGap("Bottom", stage.bottom() - centered.bottom(), stage.height(), pixels.height());
    drag(center(viewport), QPointF(stage.x() - centered.x() + 4, stage.y() - centered.y() + 4));
    check(near(geometry().x(), stage.x()) && near(geometry().y(), stage.y()) &&
              near(geometry().width(), smaller.width()) && near(geometry().height(), smaller.height()),
          "moving near two workspace edges snaps position without changing size");
    check(!findItem(preview, "recordingLeftGap")->isVisible() && !findItem(preview, "recordingTopGap")->isVisible(),
          "gap guides hide on edges without blank pixels");
    drag(center(viewport), QPointF(stage.width() - smaller.width() - 4, stage.height() - smaller.height() - 4));
    check(near(geometry().right(), stage.right()) && near(geometry().bottom(), stage.bottom()),
          "moving near the other two workspace edges snaps position");
    drag(center(viewport), QPointF(stage.left() - geometry().left() - 16, stage.top() - geometry().top() + 12));
    check(geometry().left() < stage.left() && encodedCanvas() == stage &&
              editor->property("rightGapPixels").toInt() > 0 && editor->property("bottomGapPixels").toInt() > 0,
          "moving outside the fixed encoder canvas reports the encoded blank pixels");
    const auto compositionState = [&] {
        return window->property("fake").value<QObject*>()->property("previewComposition")
            .value<QJSValue>().toVariant().toMap();
    };
    const auto composition = compositionState();
    check(composition.value("enabled").toBool() && composition.value("fit").toBool() &&
              composition.value("scale").toReal() < 1.0 && composition.value("x").toReal() < 0.5,
          "viewport geometry is sent to the production controller entry point");
    settle();
    if (violetSkin)
        window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR) + "/violet-output-crop.png");
    reset();

    struct Direction { const char* name; int x; int y; };
    const Direction directions[] = {{"topLeft", -1, -1}, {"top", 0, -1}, {"topRight", 1, -1},
        {"left", -1, 0}, {"right", 1, 0}, {"bottomLeft", -1, 1}, {"bottom", 0, 1}, {"bottomRight", 1, 1}};
    const auto anchor = [](const QRectF& r, const Direction& direction) {
        return QPointF(r.left() + (1 - direction.x) * r.width() / 2,
                       r.top() + (1 - direction.y) * r.height() / 2);
    };
    for (const auto& direction : directions) {
        auto* handle = findItem(preview, QStringLiteral("recordingViewportHandle_") + direction.name);
        check(handle && handle->isVisible(), direction.name);
        const auto start = center(handle);
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
        for (int distance : {19, 36, 0, 25}) {
            const auto sample = start + QPoint(-distance * direction.x, -(distance ? distance + 7 : 0) * direction.y);
            QTest::mouseMove(window, sample, 15);
            QCoreApplication::processEvents();
            const auto fitted = geometry();
            const auto origin = anchor(fitInitial, direction);
            const auto fixed = anchor(fitted, direction);
            check(aspectMatches(fitted) && near(fixed.x(), origin.x()) && near(fixed.y(), origin.y()),
                  "Fit preserves aspect and the opposite anchor during every drag sample");
            check(distance ? fitted.width() < fitInitial.width() && fitted.height() < fitInitial.height()
                           : near(fitted.width(), fitInitial.width()) && near(fitted.height(), fitInitial.height()),
                  "both dimensions move together and return to their original values");
        }
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, start + QPoint(-25 * direction.x, -32 * direction.y));
        reset();
    }
    auto* rightHandle = findItem(preview, "recordingViewportHandle_right");
    drag(center(rightHandle), QPointF(-42, 0));
    drag(center(rightHandle), QPointF(stage.right() - geometry().right() - 3, 0));
    check(near(geometry().right(), stage.right()) && aspectMatches(geometry()), "Fit snaps without stretching");
    drag(center(bottomRight), QPointF(-58, -30));
    const auto originalWindowSize = window->size();
    window->resize(originalWindowSize + QSize(100, 72)); settle();
    check(aspectMatches(geometry()), "Fit remains proportional when the main window changes aspect ratio");
    window->resize(originalWindowSize); settle();
    reset();

    // 填充模式继续支持两轴独立拖动，并与画布四边对齐。
    click(tool("fill"));
    const auto fullSize = [&] { return geometry() == encodedCanvas(); };
    check(fullSize(), "Fill maximizes the preview to the square canvas");
    initial = geometry();
    for (const auto& direction : directions)
    {
        auto* handle = findItem(preview, QStringLiteral("recordingViewportHandle_") + direction.name);
        check(handle && handle->isVisible(), direction.name);
        const auto start = center(handle);
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
        for (int distance : {14, 33, 21, 0, 26}) {
            const auto sample = start + QPoint(-distance * direction.x, -(distance ? distance + 5 : 0) * direction.y);
            QTest::mouseMove(window, sample, 15);
            QCoreApplication::processEvents();
            const auto resized = geometry();
            const int vertical = distance ? distance + 5 : 0;
            const auto expected = initial.adjusted(direction.x < 0 ? distance : 0, direction.y < 0 ? vertical : 0,
                                                   direction.x > 0 ? -distance : 0, direction.y > 0 ? -vertical : 0);
            const bool followsPointer = near(resized.left(), expected.left()) && near(resized.right(), expected.right()) &&
                near(resized.top(), expected.top()) && near(resized.bottom(), expected.bottom());
            if (!followsPointer)
                qWarning() << "预览拖拽" << direction.name << distance << "实际" << resized << "预期" << expected;
            check(followsPointer, "each active edge follows successive pointer samples including returning to the press point");
        }
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, start + QPoint(-26 * direction.x, -31 * direction.y));
        reset();
    }
    drag(center(bottomRight), QPointF(-4, -4));
    check(near(geometry().right(), stage.right()) && near(geometry().bottom(), stage.bottom()) &&
              near(geometry().left(), initial.left()) && near(geometry().top(), initial.top()),
          "resize snaps to both stage edges and anchors the opposite corner");
    // 导出一次带可见选中框的离屏截图。
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR) +
                            (violetSkin ? "/violet-preview-selection.png" : "/default-preview-selection.png"));
    auto* topLeft = findItem(preview, "recordingViewportHandle_topLeft");
    drag(center(topLeft), QPointF(stage.left() - geometry().left() + 4, stage.top() - geometry().top() + 4));
    check(near(geometry().left(), stage.left()) && near(geometry().top(), stage.top()), "resize snaps to the upper-left edges");
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(header));
    check(!selection->isVisible() && !selectionTools->isVisible(), "deselecting hides the editing toolbar too");
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(viewport));
    drag(center(bottomRight), QPointF(-editor->width(), -editor->height()));
    check(near(geometry().width(), editor->property("minimumWidth").toReal()) &&
              near(geometry().height(), editor->property("minimumHeight").toReal()),
          "resize clamps at a usable minimum");
    reset();
    window->resize(originalWindowSize + QSize(100, 72)); settle();
    check(fullSize(), "a maximized preview tracks the canvas when the main window resizes");
    window->resize(originalWindowSize); settle();
    click(tool("fit"));
    check(geometry() == fitInitial, "switching back to Fit restores the fitted frame");
    click(tool("crop"));
    drag(center(rightHandle), QPointF(-45, 0));
    const auto cropped = compositionState().value("crop").toRectF();
    check(cropped.width() > 0 && cropped.width() < 1 && near(geometry().left(), fitInitial.left()) &&
              near(geometry().right(), fitInitial.right() - 45), "crop handles trim source pixels without stretching");
    click(tool("rotateLeft"));
    check(loader->rotation() == 270 && compositionState().value("quarterTurns").toInt() == 3 &&
              qAbs(geometry().width() / geometry().height() - fitInitial.height() / (fitInitial.width() * cropped.width())) < 0.01,
          "counterclockwise rotation swaps the cropped aspect and reaches the encoder");
    click(tool("mirror"));
    check(compositionState().value("mirrored").toBool(), "mirror reaches the encoder through the same composition");
    click(tool("remove"));
    check(!selectionTools->isVisible() && compositionState().value("removed").toBool(), "remove blanks the active layer");
    click(findItem(preview, "recordingRestoreLayer"));
    check(selectionTools->isVisible() && !compositionState().value("removed").toBool(), "removed layer is restorable");
    click(tool("reset"));
    check(geometry() == fitInitial && loader->rotation() == 0 && !compositionState().value("mirrored").toBool() &&
              compositionState().value("crop").toRectF() == QRectF(0, 0, 1, 1), "reset restores the full upright Fit image");
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, editor->mapToScene(QPointF(2, editor->height() / 2)).toPoint());
    check(!selection->isVisible(), "clicking the stage outside the viewport clears its selection");
    for (auto* panel : outsidePanels) {
        click(viewport);
        click(tool("crop"));
        check(editor->property("cropMode").toBool(), "crop editor is active before an outside press");
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, panel->mapToScene(QPointF(10, 10)).toPoint());
        check(!selection->isVisible() && !selectionTools->isVisible() && !editor->property("cropMode").toBool(),
              "clicking another workspace panel clears selection and ends crop mode");
    }
    check(loader->property("item").value<QObject*>() == loadedPreview, "viewport editing keeps the preview renderer mounted");
}

static void checkSceneLayers(QQuickWindow* window, QQuickItem* workspace, QQuickItem* preview)
{
    auto* fixture = window->property("fake").value<QObject*>();
    const auto layers = [&] {
        return fixture->property("sceneLayers").value<QJSValue>().toVariant().toList();
    };
    const auto click = [&](QQuickItem* item) {
        check(item && item->isEnabled() && item->isVisible(), "scene control is clickable");
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                          item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
        settle();
    };
    click(findItem(workspace, "recordingAddSceneSource"));
    click(findItem(window->contentItem(), "recordingAddMonitor_1"));
    auto* upper = findItem(preview, "recordingSceneLayer_1");
    auto* main = findItem(preview, "recordingViewportEditor");
    check(upper && main && preview->property("selectedLayerId").toInt() == 1 && upper->z() > main->z(),
          "adding a source selects its independent layer above the primary display");
    const auto primary = [&] {
        for (const auto& layer : layers())
            if (layer.toMap().value("id").toInt() == 0)
                return layer.toMap();
        return QVariantMap{};
    };
    const auto initialPrimary = primary();
    const auto oldRect = upper->property("viewportRect").toRectF();
    const QPoint start = upper->mapToScene(oldRect.center()).toPoint();
    const QPoint end = start + QPoint(-60, 25);
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(window, end, 15);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, end);
    settle();
    check(oldRect.center() != upper->property("viewportRect").toRectF().center() && initialPrimary == primary(),
          "dragging the front source leaves the primary composition unchanged");
    click(findItem(preview, "recordingSelection_crop"));
    auto* handle = findItem(preview, "recordingViewportHandle_right_1");
    check(handle && handle->isVisible(), "selected layer's handles are shown above the canvas");
    const QPoint edge = handle->mapToScene(QPointF(4, 4)).toPoint();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, edge);
    QTest::mouseMove(window, edge - QPoint(40, 0), 15);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, edge - QPoint(40, 0));
    settle();
    check(layers().last().toMap().value("crop").toRectF().width() < 1 && initialPrimary == primary(),
          "cropping the selected source only updates its own encoding transform");
    click(findItem(preview, "recordingSelection_toBack"));
    check(upper->z() < main->z() && layers().first().toMap().value("id").toInt() == 1,
          "send to back changes the preview and encoder order together");
    click(findItem(preview, "recordingSelection_toFront"));
    check(upper->z() > main->z() && layers().last().toMap().value("id").toInt() == 1,
          "bring to front changes the preview and encoder order together");
    click(findItem(workspace, "recordingSceneSource_0"));
    check(preview->property("selectedLayerId").toInt() == 0 && main->z() < upper->z(),
          "the source list can select an obscured layer without moving it forward");
    const auto displayComposition = layers().last().toMap();
    click(findItem(workspace, "recordingAddSceneSource"));
    click(findItem(window->contentItem(), "recordingAddWindowSource"));
    auto* filter = findItem(window->contentItem(), "recordingWindowSourceFilter");
    check(filter && filter->isVisible(), "window picker opens from the capture source menu");
    filter->setProperty("text", "EDITOR.EXE");
    settle();
    check(!findItem(window->contentItem(), "recordingWindowCandidate_1"), "window search filters by application name");
    fixture->setProperty("rejectWindowSource", true);
    click(findItem(window->contentItem(), "recordingWindowCandidate_0"));
    check(filter->isVisible() && layers().size() == 2, "a closed window leaves the picker open and the scene unchanged");
    fixture->setProperty("rejectWindowSource", false);
    click(findItem(window->contentItem(), "recordingWindowCandidate_0"));
    auto* windowLayer = findItem(preview, "recordingSceneLayer_2");
    check(!filter->isVisible() && windowLayer && windowLayer->z() > upper->z() &&
              fixture->property("addedWindowToken").toString() == "window-one" &&
              preview->property("selectedLayerId").toInt() == 2,
          "the chosen application window joins the scene as its own selected layer");
    const auto statusChange = [&](const QString& message) {
        check(QMetaObject::invokeMethod(fixture, "sceneLayerError", Q_ARG(int, 2), Q_ARG(QString, message)),
              "window status reaches the preview");
        settle();
    };
    statusChange(QString::fromUtf8("窗口已最小化或隐藏"));
    const auto windowCenter = windowLayer->mapToScene(windowLayer->property("viewportRect").toRectF().center()).toPoint();
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, windowCenter);
    settle();
    check(preview->property("selectedLayerId").toInt() >= 0 && preview->property("selectedLayerId").toInt() != 2,
          "a paused window allows selecting an underlying display on the canvas");
    statusChange({});
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, windowCenter);
    settle();
    check(preview->property("selectedLayerId").toInt() == 2, "the resumed window can be selected in place");
    click(findItem(preview, "recordingSelection_crop"));
    auto* windowHandle = findItem(preview, "recordingViewportHandle_right_2");
    check(windowHandle && windowHandle->isVisible(), "the window uses the shared crop handles");
    const auto windowEdge = windowHandle->mapToScene(QPointF(4, 4)).toPoint();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, windowEdge);
    QTest::mouseMove(window, windowEdge - QPoint(30, 0), 15);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, windowEdge - QPoint(30, 0));
    settle();
    click(findItem(preview, "recordingSelection_toBack"));
    check(windowLayer->z() < main->z() && layers().first().toMap().value("id").toInt() == 2 &&
              layers().first().toMap().value("crop").toRectF().width() < 1 &&
              layers().last().toMap() == displayComposition && primary() == initialPrimary,
          "window crop and stacking are transmitted independently of the display layers");
    click(findItem(preview, "recordingSelection_remove"));
    check(layers().size() == 2 && layers().last().toMap() == displayComposition,
          "removing the window keeps both display layers intact");
    click(findItem(workspace, "recordingSceneSource_1"));
    click(findItem(preview, "recordingSelection_remove"));
    check(!findItem(preview, "recordingSceneLayer_1") && fixture->property("removedSceneId").toInt() == 1 &&
              layers().size() == 1 && initialPrimary == primary(), "deleting a source releases just that scene layer");
}

static void checkPersistentScenes(QQmlApplicationEngine& engine)
{
    engine.load(QUrl::fromLocalFile(QStringLiteral(APP_ROOT) + "/tests/recording-workspace/Fixture.qml"));
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().last());
    auto* workspace = findItem(window->contentItem(), "workspace");
    auto* preview = findItem(workspace, "recordingPreviewPanel");
    auto* fake = window->property("fake").value<QObject*>();
    const QVariantMap transform{{"fit", false}, {"geometry", QVariantList{0.31, 0.42, 0.59, 0.38}},
        {"crop", QVariantList{0.1, 0.15, 0.73, 0.72}}, {"scale", 0.59}, {"quarterTurns", 1}, {"mirrored", true}};
    const QVariantMap appSource{{"kind", "window"}, {"name", "Editor"}, {"windowClass", "EditorWindow"},
        {"executable", "C:/Apps/editor.exe"}, {"width", 840}, {"height", 640}, {"transform", transform}};
    const QVariantMap monitor{{"kind", "display"}, {"name", "Monitor"}, {"monitorPath", "monitor-b"},
        {"width", 2560}, {"height", 1440}, {"transform", transform}};
    const QVariantMap blank{{"id", "blank"}, {"name", "Blank"}, {"sources", QVariantList{}}};
    const QVariantMap mixed{{"id", "mix"}, {"name", "Mixed"}, {"sources", QVariantList{monitor, appSource}}};
    fake->setProperty("savedSceneLibrary", QVariantMap{{"active", "mix"}, {"scenes", QVariantList{blank, mixed}}});
    fake->setProperty("windowPresent", false);
    fake->setProperty("showSceneBackdrop", true);
    fake->setProperty("sceneLibraryEnabled", true);
    settle();
    const auto saved = [&] { return fake->property("savedSceneLibrary").value<QJSValue>().toVariant().toMap(); };
    const auto layers = [&] { return fake->property("sceneLayers").value<QJSValue>().toVariant().toList(); };
    check(preview->property("currentSceneName") == "Mixed" && layers().size() == 3, "restart restores the active scene and its two sources");
    auto* legacyMonitor = findItem(workspace, "recordingMonitor");
    check(!legacyMonitor || !legacyMonitor->isVisible(), "managed scenes omit the old canvas monitor selector");
    const auto windowLayer = layers().last().toMap();
    const auto geometry = windowLayer.value("geometry").toRectF();
    const auto crop = windowLayer.value("crop").toRectF();
    check(qAbs(geometry.x() - 0.31) < 0.001 && qAbs(geometry.width() - 0.59) < 0.001 &&
          qAbs(crop.width() - 0.73) < 0.001 && windowLayer.value("quarterTurns").toInt() == 1 &&
          windowLayer.value("mirrored").toBool(), "restored waiting source keeps its exact geometry and crop");
    auto* sourceRow = findItem(workspace, "recordingSceneSource_2");
    check(sourceRow && sourceRow->property("text").toString().contains(QString::fromUtf8("待连接")),
          "closed window is listed as waiting for reconnect");
    check(QMetaObject::invokeMethod(fake, "sceneLayerError", Q_ARG(int, 2), Q_ARG(QString, QString())),
          "window reconnect status updates its existing layer");
    settle();
    sourceRow = findItem(workspace, "recordingSceneSource_2");
    check(sourceRow && !sourceRow->property("text").toString().contains(QString::fromUtf8("待连接")) &&
          windowLayer == layers().last().toMap(), "reattachment preserves the scene and composition");

    const auto click = [&](QQuickItem* button) {
        check(button && button->isVisible() && button->isEnabled(), "scene control is available");
        const auto point = button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint();
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point);
        settle();
    };
    const auto* canvas = findItem(preview, "recordingPreviewCanvas");
    const auto canvasSize = canvas->size();
    click(sourceRow);
    auto* options = findItem(preview, "recordingSourceOptions");
    check(options && options->isVisible() && canvas->size() == canvasSize &&
          findItem(options, "recordingSourceValue")->property("text") == "Editor" &&
          findItem(options, "recordingReplaceSource")->isVisible(), "window options appear without resizing the canvas");
    click(findItem(workspace, "recordingSourceLock_2"));
    auto* editor = findItem(preview, "recordingSceneLayer_2");
    check(editor && editor->property("sourceLocked").toBool() && options->isVisible(), "lock retains the selected source");
    click(findItem(workspace, "recordingSourceVisibility_2"));
    check(editor->property("sourceHidden").toBool() && layers().last().toMap().value("removed").toBool(),
          "hiding a source also removes it from encoded composition");
    click(findItem(workspace, "recordingSourceVisibility_2"));
    click(findItem(workspace, "recordingSourceLock_2"));
    click(findItem(workspace, "recordingSceneSource_1"));
    auto* monitorChoice = findItem(options, "recordingSourceMonitor");
    check(monitorChoice && monitorChoice->isVisible() && !findItem(options, "recordingReplaceSource")->isVisible(),
          "monitor selection switches the contextual controls");
    check(QMetaObject::invokeMethod(monitorChoice, "activated", Q_ARG(int, 0)), "replace a monitor from its contextual selector");
    settle();
    check(layers()[1].toMap().value("id") != 1 && layers()[1].toMap().value("crop").toRectF() == crop &&
          layers().last().toMap().value("id") == 2, "changing a monitor preserves position, crop and stacking");

    QTemporaryDir directory;
    const auto imagePath = directory.filePath("scene.png");
    QImage image(128, 96, QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            image.setPixelColor(x, y, QColor(255, 0, 0, x < 40 ? 0 : x < 88 ? 128 : 255));
    check(directory.isValid() && image.save(imagePath), "write an isolated scene image");
    // 把已有显示器源铺满画布，以便在真实的下层源上检查 PNG 的合成结果。
    const auto monitorId = layers()[1].toMap().value("id").toInt();
    auto* backdrop = findItem(preview, "recordingSceneLayer_" + QString::number(monitorId));
    auto fullTransform = transform;
    fullTransform["geometry"] = QVariantList{0.5, 0.5, 1, 1};
    fullTransform["crop"] = QVariantList{0, 0, 1, 1};
    fullTransform["quarterTurns"] = 0;
    fullTransform["mirrored"] = false;
    check(backdrop && QMetaObject::invokeMethod(backdrop, "restoreComposition", Q_ARG(QVariant, QVariant(fullTransform))),
          "make the display source cover the preview canvas");
    QVariant added;
    const QVariant url = QUrl::fromLocalFile(imagePath);
    check(QMetaObject::invokeMethod(preview, "addImageSource", Q_RETURN_ARG(QVariant, added),
              Q_ARG(QVariant, url), Q_ARG(QVariant, QVariant(-1))) && added.toBool(), "add an image as a scene source");
    settle();
    check(layers().size() == 4 && findItem(options, "recordingSourceValue")->property("text") == imagePath &&
          findItem(options, "recordingReplaceSource")->isVisible(), "image source shows its file and browse control");
    const auto imageId = layers().last().toMap().value("id").toInt();
    auto* previewImage = findItem(preview, "recordingSceneImage_" + QString::number(imageId));
    check(previewImage && previewImage->isVisible(), "preview decoded the transparent PNG");
    check(QMetaObject::invokeMethod(preview, "clearSelection"), "hide editing guides for pixel sampling");
    settle();
    const auto composited = window->grabWindow();
    const auto colorAt = [&](double x) {
        // 在图块内部取样，避开变换框和间距标尺。
        const auto position = previewImage->mapToScene({previewImage->width() * x, previewImage->height() * 0.8});
        return composited.pixelColor((position * composited.devicePixelRatio()).toPoint());
    };
    const auto half = colorAt(0.5);
    check(colorAt(0.18) == QColor(Qt::white) && colorAt(0.82) == QColor(Qt::red) &&
          half.red() == 255 && qAbs(half.green() - 127) <= 1 && qAbs(half.blue() - 127) <= 1,
          "workspace preview composites transparent and half-transparent PNG pixels over a display source");
    check(QMetaObject::invokeMethod(preview, "selectSceneLayer", Q_ARG(QVariant, QVariant(imageId))),
          "reselect the temporary image");
    check(QMetaObject::invokeMethod(preview, "removeActiveLayer"), "remove the temporary image from the scene");
    settle();
    check(!options->isVisible() && canvas->size() == canvasSize, "deselect hides contextual controls without resizing");

    const auto select = [&](int id) {
        check(QMetaObject::invokeMethod(preview, "selectSceneLayer", Q_ARG(QVariant, QVariant(id))),
              "select a scene source for toolbar collision checks");
        settle();
    };
    auto* transformTools = findItem(preview, "recordingSelectionTools");
    auto compactTransform = fullTransform;
    compactTransform["geometry"] = QVariantList{0.12, 0.14, 0.18, 0.24};
    check(QMetaObject::invokeMethod(editor, "restoreComposition", Q_ARG(QVariant, QVariant(compactTransform))),
          "place a small window against the top left of the stage");
    select(2);
    const auto compactRect = editor->property("viewportRect").toRectF();
    check(transformTools && transformTools->isVisible() && transformTools->x() > editor->mapToItem(preview, compactRect.topRight()).x(),
          "floating toolbar moves sideways to leave the complete top edge clear");
    check(QMetaObject::invokeMethod(editor, "restoreComposition", Q_ARG(QVariant, QVariant(transform))),
          "restore the saved window transform");

    select(monitorId);
    const auto fullRect = backdrop->property("viewportRect").toRectF();
    const auto topEdge = backdrop->mapToScene({fullRect.x() + fullRect.width() * 0.32, fullRect.y()}).toPoint();
    auto* topHandle = findItem(preview, "recordingViewportHandle_top_" + QString::number(monitorId));
    auto* headerInfo = findItem(preview, "recordingPreviewHeaderInfo");
    check(topHandle && topHandle->isVisible() && headerInfo && headerInfo->isVisible(),
          "top handle and preview header remain visible");
    const auto toolsTop = transformTools->mapToItem(canvas, QPointF()).y();
    const auto handleBottom = topHandle->mapToItem(canvas, QPointF(0, topHandle->height())).y();
    check(transformTools->isVisible() && toolsTop > handleBottom && toolsTop + transformTools->height() <= canvas->height() &&
          options->isVisible() && canvas->size() == canvasSize,
          "full-width source keeps the toolbar below the top handle inside the fixed canvas");
    auto* cropTool = findItem(transformTools, "recordingSelection_crop");
    click(cropTool);
    check(backdrop->property("cropMode").toBool(), "toolbar actions work below the top handle");
    click(cropTool);
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, topEdge);
    QTest::mouseMove(window, topEdge + QPoint(0, 50), 15);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, topEdge + QPoint(0, 50));
    settle();
    const auto resized = backdrop->property("viewportRect").toRectF();
    check(qAbs(resized.top() - fullRect.top() - 50) < 2 && qAbs(resized.bottom() - fullRect.bottom()) < 2,
          "dragging the top border away from the center handle resizes the selected source");

    click(findItem(workspace, "recordingScene_blank"));
    settle();
    check(layers().size() == 1 && layers().first().toMap().value("removed").toBool() && saved().value("active") == "blank",
          "blank scene publishes a blank canvas and persists the selected scene");
    click(findItem(workspace, "recordingScene_mix"));
    settle();
    check(layers().size() == 3 && layers().last().toMap().value("crop").toRectF() == crop,
          "switching scenes restores their own sources and crop");
    auto* oldAdd = findItem(workspace, "recordingAddSceneSource");
    auto* settingsButton = findItem(workspace, "recordingOpenSceneSettings");
    check(oldAdd && !oldAdd->isVisible() && settingsButton && QMetaObject::invokeMethod(settingsButton, "clicked"),
          "adding sources moves from the source list into scene settings");
    settle();
    auto* dialogAdd = findItem(window->contentItem(), "recordingSceneSettingsAddSource");
    auto* create = findItem(window->contentItem(), "recordingCreateScene");
    check(dialogAdd && dialogAdd->isVisible() && create && QMetaObject::invokeMethod(create, "clicked"),
          "scene settings provide adding and creating scenes");
    settle();
    auto* name = findItem(window->contentItem(), "recordingSceneName");
    auto* rename = findItem(window->contentItem(), "recordingRenameScene");
    check(name && rename, "new scene exposes its name");
    name->setProperty("text", "Third scene");
    check(QMetaObject::invokeMethod(rename, "clicked"), "rename the new scene");
    settle();
    check(preview->property("currentSceneName") == "Third scene" && saved().value("scenes").toList().size() == 3,
          "scene creation and name are persisted");
    window->close();
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen"); qputenv("QT_QUICK_BACKEND", "software"); qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");
    QGuiApplication app(argc, argv);
    check(QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR", "C:/Windows") + "/Fonts/msyh.ttc") >= 0, "offscreen Chinese font loaded");
    QFont::insertSubstitution(QString::fromUtf8("微软雅黑"), "Microsoft YaHei");
    app.setFont(QFont("Microsoft YaHei"));
    check(Oran7PointerParallax::offsetForPosition({500, 300}, {1000, 600}, 30) == QPointF(), "parallax centered at viewport center");
    check(Oran7PointerParallax::offsetForPosition({1000, 600}, {1000, 600}, 30) == QPointF(-30, -30), "parallax reverses pointer direction");
    check(Oran7PointerParallax::offsetForPosition({-4000, 9000}, {1000, 600}, 30) == QPointF(30, -30), "global pointer outside window stays bounded");
    check(Oran7PointerParallax::offsetForPosition({500, 300}, {0, 0}, 30) == QPointF(), "empty viewport has no offset");
    qmlRegisterType<Oran7PointerParallax>("Oran7UI.Impl", 1, 0, "Oran7PointerParallax");
    qmlRegisterType<Oran7ProcessLoadMonitor>("Client", 1, 0, "Oran7ProcessLoadMonitor");
    QQmlPropertyMap theme;
    theme.insert("isDark",true);
    theme.insert("Primary", QVariantMap{{"durationMid", 100}, {"durationVerySlow", 100}, {"durationFast", 100}});
    theme.insert("Oran7MainGUI", QVariantMap{{"themeColor", "#beb7ff"}, {"isDarkMode", true}});
    theme.insert("Oran7ScreenCapture", QVariantMap{{"ScaleMode", "Fit"}});
    qmlRegisterSingletonInstance("Oran7UI.Impl", 1, 0, "Oran7Theme", &theme);
    qmlRegisterSingletonType(QUrl::fromLocalFile(QStringLiteral(APP_ROOT) + "/src/qml/Settings/GlobalSettings/Oran7MainUiSetting.qml"), "Oran7MainUiSetting", 1, 0, "Oran7MainUiSetting");
    const auto fakeClientId = qmlRegisterSingletonType(QUrl::fromLocalFile(QStringLiteral(APP_ROOT) + "/tests/recording-workspace/FakeClient.qml"), "Client", 1, 0, "Client");
    QQmlApplicationEngine engine;
    QStringList warnings;
    QObject::connect(&engine, &QQmlEngine::warnings, [&](const QList<QQmlError>& list) { for (const auto& error : list) { warnings << error.toString(); std::fprintf(stderr, "%s\n", qPrintable(error.toString())); } });
    engine.load(QUrl::fromLocalFile(QStringLiteral(APP_ROOT) + "/tests/recording-workspace/Fixture.qml"));
    check(!engine.rootObjects().isEmpty(), "workspace loads");
    settle();
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    check(window, "offscreen window");
    auto* workspace = findItem(window->contentItem(), "workspace");
    auto* parallax = qobject_cast<Oran7PointerParallax*>(findItem(workspace, "recordingAvatarParallax"));
    check(parallax, "native cursor tracker instantiated");
    parallax->setActive(false);
    check(!parallax->tracking() && parallax->offset().isNull(), "disabled tracker stops polling and resets offset");
    auto* source = findItem(workspace, "recordingSourcePanel");
    auto* output = findItem(workspace, "recordingOutputPanel");
    auto* preview = findItem(workspace, "recordingPreviewPanel");
    auto* center = findItem(workspace, "recordingCenterColumn");
    auto* toolbar = findItem(workspace, "recordingCenterToolbar");
    check(source && output && preview, "three panels instantiated");
    auto* loadStrip = findItem(workspace, "recordingProcessLoad");
    auto* restoreSettings = findItem(workspace, "recordingRestoreSettings");
    auto* cpuLoad = findItem(workspace, "recordingCpuLoadValue");
    auto* gpuLoad = findItem(workspace, "recordingGpuLoadValue");
    check(loadStrip && restoreSettings && cpuLoad && gpuLoad
          && loadStrip->mapToItem(restoreSettings, QPointF(loadStrip->width(), 0)).x() <= 0,
          "process load strip is immediately left of restore settings");
    check(cpuLoad->property("text") == QStringLiteral("—") && gpuLoad->property("text") == QStringLiteral("—"),
          "unavailable samples display placeholders");
    workspace->setProperty("processCpuPercent", 80);
    workspace->setProperty("processGpuPercent", 79);
    settle();
    check(cpuLoad->property("text") == QStringLiteral("80%") && gpuLoad->property("text") == QStringLiteral("79%")
          && cpuLoad->property("color") == workspace->property("errorColor")
          && gpuLoad->property("color") == workspace->property("textColor"),
          "only the number at or above the high-load threshold is highlighted");
    workspace->setProperty("processCpuPercent", 23);
    workspace->setProperty("processGpuPercent", 94);
    settle();
    check(cpuLoad->property("color") == workspace->property("textColor")
          && gpuLoad->property("color") == workspace->property("errorColor"),
          "CPU and GPU highlights update independently");
    auto* fileFormat = findItem(workspace, "recordingFileFormat");
    check(fileFormat && fileFormat->property("currentIndex").toInt() == 0, "local format defaults to MP4");
    QMetaObject::invokeMethod(fileFormat, "activated", Q_ARG(int, 1)); settle(); QTest::qWait(100);
    auto* captureFixture = window->property("fake").value<QObject*>();
    check(captureFixture && captureFixture->property("options").value<QJSValue>().toVariant().toMap().value("format") == "flv", "FLV selection automatically saved");
    QMetaObject::invokeMethod(fileFormat, "activated", Q_ARG(int, 0)); settle(); QTest::qWait(100);
    check(captureFixture->property("options").value<QJSValue>().toVariant().toMap().value("format") == "mp4", "can switch back to MP4");
    check(center && toolbar && source->x() < center->x() && center->x() < output->x() && preview->width() >= 320, "wide studio layout");
    check(toolbar->parentItem() == preview->parentItem() && toolbar->y() >= preview->y() + preview->height()
        && toolbar->width() == preview->width(), "toolbar belongs directly beneath center preview");
    auto* previewBadge = findItem(preview, "recordingPreviewBadge");
    auto* previewTools = findItem(preview, "recordingSelectionTools");
    check(previewBadge->mapToItem(preview, QPointF(previewBadge->width(), 0)).x() <= preview->width() - 6,
        "preview status badge remains inside the frame");
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/wide.png");
    check(!workspace->property("violetSkin").toBool(), "default preview editing is exercised before switching skins");
    checkViewportEditing(window, preview, {source, toolbar, output});
    checkSceneLayers(window, workspace, preview);
    const auto initialPanelColor = workspace->property("panelColor").value<QColor>();
    const QList<QQuickItem*> skinLayoutItems{source, output, preview, toolbar};
    QList<QRectF> skinRects;
    for (auto* part : skinLayoutItems) skinRects.append(QRectF(part->position(), part->size()));
    // 皮肤切换必须保持混音器和内部控件的布局一致，宽／窄窗口都比较默认基线。
    auto mixerGeometry = [&] {
        QList<QRectF> result;
        for (const auto* name : {"recordingSystemAudioTool", "recordingMicrophoneTool", "recordingAudio", "recordingMicrophone",
                                 "recordingSystemVolume", "recordingMicrophoneVolume", "recordingSystemLevelMeter", "recordingMicrophoneLevelMeter",
                                 "recordingControlRow", "recordingStatusRow"}) {
            auto* item = findItem(toolbar, name);
            result.append(QRectF(item->mapToItem(toolbar, QPointF()), item->size()));
        }
        return result;
    };
    const auto defaultMixerWide = mixerGeometry();
    window->setWidth(850); settle();
    const auto defaultMixerNarrow = mixerGeometry();
    window->setWidth(1320); settle();
    const int captureAppliesBeforeSkin = captureFixture->property("captureApplies").toInt();
    theme.insert("Oran7ScreenCapture", QVariantMap{{"ScaleMode", "Fit"}, {"workspaceSkin", "violet"}});
    settle();
    check(workspace->property("violetSkin").toBool()
        && workspace->property("panelColor").value<QColor>() != initialPanelColor, "violet skin applies without recreating workspace");
    auto* snowfield = findItem(workspace, "recordingVioletLandscape");
    auto* roundedSnowfield = findItem(workspace, "recordingRoundedSnowfield");
    auto* workspaceBackground = workspace->property("background").value<QQuickItem*>();
    check(snowfield && snowfield->property("status").toInt() == 1 && !snowfield->isVisible()
          && workspaceBackground && workspaceBackground->property("color").value<QColor>() == QColor("#ffffff")
          && roundedSnowfield && roundedSnowfield->isVisible() && roundedSnowfield->size() == workspace->size(),
          "rounded snowfield and frosted cards share the image over a white fallback");
    auto* frost = findItem(workspace, "recordingFrostedCards");
    auto frostMatchesSnowfield = [&] {
        const QRectF sampled = frost->property("backdropRect").toRectF();
        const QSizeF painted(snowfield->property("paintedWidth").toReal(),
                             snowfield->property("paintedHeight").toReal());
        const QPointF imageOrigin((snowfield->width() - painted.width()) / 2,
                                  (snowfield->height() - painted.height()) / 2);
        const auto offset = sampled.topLeft() - snowfield->mapToItem(frost, imageOrigin);
        return sampled.size() == painted && qAbs(offset.x()) < 1 && qAbs(offset.y()) < 1;
    };
    check(frost && frost->isVisible() && frostMatchesSnowfield(), "frost retains the shared snowfield framing");
    auto* avatar = findItem(workspace, "recordingStudioAvatar");
    check(avatar && avatar->isVisible() && avatar->opacity() > 0.45
          && workspace->property("avatarTintColor").value<QColor>() == QColor("#51436f"),
          "violet skin retains a higher-contrast avatar outline");
    for (int i = 0; i < skinLayoutItems.size(); ++i)
        check(QRectF(skinLayoutItems[i]->position(), skinLayoutItems[i]->size()) == skinRects[i], "skin leaves all panel geometry unchanged");
    check(mixerGeometry() == defaultMixerWide, "violet mixer matches default control layout in wide windows");
    check(captureFixture->property("captureApplies").toInt() == captureAppliesBeforeSkin,
        "skin selection does not reconfigure capture");
    checkViewportEditing(window, preview, {source, toolbar, output});
    check(captureFixture->property("captureApplies").toInt() == captureAppliesBeforeSkin,
          "moving and resizing the viewport does not reconfigure capture");
    // 两种表皮都走真实鼠标输入，防止新增装饰拦截点击或委托绑定在第一次点击后失效。
    auto* skinAudio = findItem(workspace, "recordingAudio");
    const bool initialAudioEnabled = skinAudio->property("checked").toBool();
    QSignalSpy skinAudioEdits(skinAudio, SIGNAL(switchToggleChanged(bool)));
    auto clickSkinAudio = [&] {
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                         skinAudio->mapToScene(QPointF(skinAudio->width()/2, skinAudio->height()/2)).toPoint());
        settle();
    };
    clickSkinAudio();
    check(skinAudioEdits.count() == 1 && skinAudio->property("checked").toBool() != initialAudioEnabled
        && toolbar->property("systemEnabled").toBool() != initialAudioEnabled,
        "violet switch click emits once and updates actual audio options");
    skinAudio->forceActiveFocus(); QTest::keyClick(window, Qt::Key_Space); settle();
    check(skinAudioEdits.count() == 2 && skinAudio->property("checked").toBool() == initialAudioEnabled,
        "violet switch keyboard activation restores audio state");
    const QImage violetWide = window->grabWindow();
    const int rightEdge = violetWide.width() - 1, bottomEdge = violetWide.height() - 1;
    const QColor windowColor = window->color();
    check(violetWide.pixelColor(0, 0) == windowColor && violetWide.pixelColor(rightEdge, 0) == windowColor
        && violetWide.pixelColor(0, bottomEdge) == windowColor && violetWide.pixelColor(rightEdge, bottomEdge) == windowColor
        && violetWide.pixelColor(rightEdge / 2, 0) != windowColor, "snowfield and tint leave all four corners transparent");
    check(violetWide.pixelColor(5, violetWide.height() / 2) != QColor("#ffffff"),
          "workspace margin displays the snowfield instead of a blank white base");
    violetWide.save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/violet-wide.png");
    auto* standby = findItem(preview, "recordingPreviewStandby");
    check(standby && !standby->isVisible(), "working capture has no standby overlay");
    captureFixture->setProperty("previewState", "Idle"); settle();
    check(standby->isVisible() && standby->property("color") != preview->property("canvasColor"),
          "violet standby uses a distinct canvas");
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/violet-standby.png");
    captureFixture->setProperty("previewState", "Running"); settle();
    window->setWidth(850); settle();
    check(mixerGeometry() == defaultMixerNarrow, "violet mixer matches default control layout in narrow windows");
    auto* monitor = findItem(workspace, "recordingMonitor");
    auto* startRecording = findItem(workspace, "startLocalRecording");
    auto* status = findItem(toolbar, "recordingStatus");
    const auto pixelSize = [](QQuickItem* item) { return item->property("font").value<QFont>().pixelSize(); };
    check(monitor && startRecording && status && pixelSize(monitor) >= 15 && pixelSize(startRecording) >= 15 &&
              pixelSize(status) >= 13, "workspace fields, actions and mixer status use readable default text sizes");
    const auto defaultControlHeight = startRecording->height();
    workspace->setProperty("textPixelSize", 24);
    settle();
    auto* actionText = startRecording->property("contentItem").value<QQuickItem*>();
    check(pixelSize(monitor) > 15 && pixelSize(startRecording) > 15 && pixelSize(status) > 13,
          "large text preferences propagate across narrow workspace panels");
    if (actionText && actionText->property("contentHeight").toReal() > actionText->height())
        qWarning() << "录制按钮文字高度" << actionText->property("contentHeight") << "可用高度" << actionText->height();
    check(startRecording->height() > defaultControlHeight && actionText &&
              actionText->property("contentHeight").toReal() <= actionText->height(),
          "the output action grows with larger text without clipping");
    workspace->setProperty("textPixelSize", 16);
    settle();
    check(qAbs(findItem(toolbar, "recordingMicrophoneTool")->y()-findItem(toolbar, "recordingSystemAudioTool")->y()) < 1,
          "both skins keep audio channels on the same row in narrow windows");
    auto* violetAudioTools = findItem(toolbar, "recordingAudioTools");
    auto* violetAudioFlick = violetAudioTools->property("contentItem").value<QObject*>();
    violetAudioFlick->setProperty("contentX", violetAudioFlick->property("contentWidth").toReal()-violetAudioFlick->property("width").toReal());
    settle();
    auto* violetMicrophone = findItem(toolbar, "recordingMicrophone");
    check(violetMicrophone->mapToItem(violetAudioTools, QPointF(violetMicrophone->width(), 0)).x() <= violetAudioTools->width(),
          "narrow violet mixer keeps microphone reachable by scrolling");
    violetAudioFlick->setProperty("contentX", 0); settle();
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/violet-narrow.png");
    auto* outerScroll = findItem(workspace, "recordingWorkspaceScroll");
    auto* horizontalBar = findItem(outerScroll, "recordingWorkspaceHorizontalScrollBar");
    check(horizontalBar && horizontalBar->isVisible() && horizontalBar->height() >= 6
        && horizontalBar->mapToItem(outerScroll, QPointF(0, horizontalBar->height())).y() <= outerScroll->height(),
          "overflowing narrow workspace shows its horizontal scrollbar inside the viewport");
    auto* outerFlick = outerScroll->property("contentItem").value<QObject*>();
    outerFlick->setProperty("contentX", outerFlick->property("contentWidth").toReal()-outerFlick->property("width").toReal());
    settle();
    check(frostMatchesSnowfield(), "frost stays aligned when the narrow workspace scrolls");
    check(output->mapToItem(outerScroll, QPointF()).x() >= 0
        && output->mapToItem(outerScroll, QPointF(output->width(), 0)).x() <= outerScroll->width(),
          "narrow horizontal scroll reaches the entire output panel");
    window->setHeight(440); settle();
    check(findItem(workspace, "recordingOutputPanelScroll")->height() >= 100,
          "short workspace reserves a usable output settings viewport above action cards");
    outerFlick->setProperty("contentY", outerFlick->property("contentHeight").toReal()-outerFlick->property("height").toReal());
    settle();
    for (const auto* name : {"startLocalRecording", "stopLocalRecording", "startLiveStreaming"}) {
        auto* action = findItem(workspace, name);
        const auto topLeft = action->mapToItem(outerScroll, QPointF());
        const auto bottomRight = action->mapToItem(outerScroll, QPointF(action->width(), action->height()));
        check(topLeft.x() >= 0 && topLeft.y() >= 0 && bottomRight.x() <= outerScroll->width() && bottomRight.y() <= outerScroll->height(),
              "short narrow workspace keeps output actions reachable without clipping");
    }
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/violet-narrow-output.png");
    window->setHeight(780); settle();
    outerFlick->setProperty("contentX", 0); outerFlick->setProperty("contentY", 0); settle();
    window->setWidth(1320); settle();
    theme.insert("Oran7ScreenCapture", QVariantMap{{"ScaleMode", "Fit"}, {"workspaceSkin", "default"}});
    settle();
    check(!workspace->property("violetSkin").toBool() && workspace->property("panelColor").value<QColor>() == initialPanelColor,
        "default skin restores exact original palette");
    check(avatar->isVisible() && !roundedSnowfield->isVisible() && !frost->isVisible(),
        "default skin restores original decoration");
    theme.insert("Oran7ScreenCapture", QVariantMap{{"ScaleMode", "Fit"}, {"workspaceSkin", "violet"}});
    settle();
    check(window->grabWindow().pixelColor(5, window->height() / 2) == violetWide.pixelColor(5, violetWide.height() / 2)
          && snowfield->property("status").toInt() == 1 && !snowfield->isVisible()
          && roundedSnowfield->isVisible() && frost->isVisible() && frostMatchesSnowfield() && avatar->isVisible(),
          "returning to snowfield skin repaints the same landscape, frosted cards and avatar");
    theme.insert("Oran7ScreenCapture", QVariantMap{{"ScaleMode", "Fit"}, {"workspaceSkin", "default"}});
    settle();
    clickSkinAudio();
    check(skinAudioEdits.count() == 3 && skinAudio->property("checked").toBool() != initialAudioEnabled,
        "default switch retains original mouse interaction after changing skins");
    QMetaObject::invokeMethod(workspace, "editLiveAudio", Q_ARG(QVariant, QString("audioEnabled")), Q_ARG(QVariant, initialAudioEnabled));
    settle();
    clickSkinAudio();
    check(skinAudioEdits.count() == 4 && skinAudio->property("checked").toBool() != initialAudioEnabled,
        "default switch reflects external option changes after a previous click");
    clickSkinAudio();
    check(skinAudio->property("checked").toBool() == initialAudioEnabled, "switch checks restore initial audio options");
    auto* pageState = workspace->property("pageState").value<QObject*>();
    auto draft = [&](const char* name) { return pageState->property(name).value<QJSValue>().toVariant().toMap(); };
    auto click = [&](const char* name) {
        auto* button = findItem(workspace, name);
        check(button && QMetaObject::invokeMethod(button, "clicked"), name);
        settle();
    };
    auto edit = [&](const char* name, const QString& value) {
        auto* field = findItem(workspace, name);
        field->setProperty("text", value); QMetaObject::invokeMethod(field, "textEdited"); settle();
    };
    check(!findItem(workspace, "recordingAdvancedGroup")->isVisible(), "advanced diagnostics initially collapsed");
    // clicked 信号的调用不模拟 Button 自己的 checkable 切换，显式设置后检查内容展开。
    findItem(workspace, "recordingAdvancedToggle")->setProperty("checked", true); settle();
    check(findItem(workspace, "recordingAdvancedGroup")->isVisible(), "advanced diagnostics can expand");
    findItem(workspace, "recordingAdvancedToggle")->setProperty("checked", false);
    check(!findItem(workspace, "captureFpsChoicesOption0")->isEnabled(), "capture rate below recording cap is disabled");
    click("captureFpsChoicesOption2");
    check(draft("captureDraft").value("fps").toInt() == 120 && !findItem(workspace, "captureCustomFps")->isVisible(), "capture shortcut edits existing draft");
    click("captureFpsChoicesOption3");
    check(findItem(workspace, "captureCustomFps")->isVisible() && draft("captureDraft").value("fps").toInt() == 120, "custom capture rate expands without changing value");
    edit("captureCustomFps", "75");
    check(draft("captureDraft").value("fps").toInt() == 75, "custom capture frame rate retained");
    edit("captureCustomFps", "");
    QVariant applyResult;
    QMetaObject::invokeMethod(pageState, "applyCapture", Q_RETURN_ARG(QVariant, applyResult));
    check(!applyResult.toBool() && !pageState->property("applying").toBool(), "empty custom capture rate is rejected before applying");
    click("captureFpsChoicesOption1");
    check(!findItem(workspace, "captureCustomFps")->isVisible(), "capture preset collapses custom input");
    auto* sizeChoice = findItem(workspace, "recordingSizePreset");
    QMetaObject::invokeMethod(sizeChoice, "activated", Q_ARG(int, 1)); settle();
    check(draft("outputDraft").value("width").toInt() == 1920 && draft("outputDraft").value("height").toInt() == 1080
        && sizeChoice->property("currentIndex").toInt() == 1 && !findItem(workspace, "recordingCustomSize")->isVisible(), "1080p shortcut submits both dimensions and synchronizes selection");
    QMetaObject::invokeMethod(sizeChoice, "activated", Q_ARG(int, 3)); settle();
    check(findItem(workspace, "recordingCustomSize")->isVisible(), "custom dimensions expand for matching preset values");
    QMetaObject::invokeMethod(sizeChoice, "activated", Q_ARG(int, 0)); settle();
    check(draft("outputDraft").value("width").toInt() == 0 && draft("outputDraft").value("height").toInt() == 0, "original size preserves backend zero convention");
    click("outputFpsChoicesOption2"); edit("recordingFps", "50");
    check(draft("outputDraft").value("fps").toInt() == 50 && findItem(workspace, "recordingFps")->isVisible(), "nonstandard output rate stays editable");
    click("outputFpsChoicesOption1");
    check(draft("outputDraft").value("fps").toInt() == 60 && !findItem(workspace, "recordingFps")->isVisible(), "output rate preset restores compact view");
    auto* bitrateChoice = findItem(workspace, "recordingBitratePreset");
    check(bitrateChoice->property("currentIndex").toInt() == 4 && !findItem(workspace, "recordingBitrate")->isVisible(), "existing 40 Mbps selects matching tier without rewriting settings");
    auto* bitratePopup = bitrateChoice->property("popup").value<QObject*>();
    QMetaObject::invokeMethod(bitratePopup, "open"); settle();
    check(bitratePopup->property("visible").toBool() && bitratePopup->property("width").toReal() >= 350, "bitrate popup opens with room for title and description");
    auto popupFits = [&] {
        auto* content = qobject_cast<QQuickItem*>(bitratePopup->property("contentItem").value<QObject*>());
        const auto topLeft = content->mapToScene(QPointF());
        const auto bottomRight = content->mapToScene(QPointF(content->width(), content->height()));
        return topLeft.x() >= 0 && topLeft.y() >= 0 && bottomRight.x() <= window->width() && bottomRight.y() <= window->height();
    };
    check(popupFits(), "expanded bitrate descriptions stay inside wide window");
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/picker.png");
    QMetaObject::invokeMethod(bitratePopup, "close");
    QMetaObject::invokeMethod(bitrateChoice, "activated", Q_ARG(int, 1)); settle();
    check(draft("outputDraft").value("bitrateKbps").toInt() == 8000
        && workspace->property("bitrateStorageEstimate").toString().contains("3.6 GB"), "8 Mbps writes integer kbps and estimates decimal GB per hour");
    QMetaObject::invokeMethod(bitrateChoice, "activated", Q_ARG(int, 6)); settle();
    check(findItem(workspace, "recordingBitrate")->isVisible() && draft("outputDraft").value("bitrateKbps").toInt() == 8000, "custom tier reveals current value without overwriting draft");
    edit("recordingBitrate", "0.1");
    check(workspace->property("bitrateStorageEstimate").toString().contains("45 MB"), "low bitrate estimate avoids rounding to zero GB");
    edit("recordingBitrate", "12.345");
    check(draft("outputDraft").value("bitrateKbps").toInt() == 12345, "Mbps input converts exactly to existing integer kbps");
    workspace->setProperty("customBitrate", false); settle();
    check(bitrateChoice->property("currentIndex").toInt() == 6 && findItem(workspace, "recordingBitrate")->isVisible(), "nonstandard saved bitrate automatically uses custom tier");
    QMetaObject::invokeMethod(bitrateChoice, "activated", Q_ARG(int, 4)); settle();
    check(draft("outputDraft").value("bitrateKbps").toInt() == 40000 && !findItem(workspace, "recordingBitrate")->isVisible(), "choosing preset restores compact bitrate control");
    const QString longPath = "C:/very-long-recordings-directory/session/archive";
    edit("recordingDirectory", longPath);
    check(draft("outputDraft").value("outputDirectory").toString() == longPath
        && findItem(workspace, "recordingDirectoryDisplay")->property("elide").toInt() == Qt::ElideMiddle, "path remains complete in draft with middle elision in display");
    workspace->setProperty("customCaptureFps", true); workspace->setProperty("customOutputFps", true); workspace->setProperty("customOutputSize", true);
    findItem(workspace, "recordingAdvancedToggle")->setProperty("checked", true); settle();
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/custom.png");
    findItem(workspace, "recordingAdvancedToggle")->setProperty("checked", false);
    QMetaObject::invokeMethod(pageState, "reset");
    workspace->setProperty("customCaptureFps", false); workspace->setProperty("customOutputFps", false); workspace->setProperty("customOutputSize", false); workspace->setProperty("customBitrate", false);
    settle();
    // 实际打开下拉列表，覆盖委托创建及未连接显示器项，而不只检查静态布局。
    auto* monitorChoice = findItem(workspace, "recordingMonitor");
    check(monitorChoice->property("displayText").toString() == QString::fromUtf8("显示器 5：X7IQ"), "monitor name appends hardware model while retaining original output index");
    const auto monitors = pageState->property("monitors").value<QJSValue>().toVariant().toList();
    check(monitors.last().toMap().value("name").toString() == QString::fromUtf8("显示器 8"), "missing hardware model keeps plain display label");
    auto* monitorPopup = monitorChoice->property("popup").value<QObject*>();
    QMetaObject::invokeMethod(monitorPopup, "open"); settle();
    check(monitorPopup->property("visible").toBool(), "monitor picker opens with detail delegates");
    QMetaObject::invokeMethod(monitorPopup, "close");
    window->setHeight(520); settle();
    check(!findItem(workspace, "applyCaptureSettings") && !findItem(workspace, "saveRecordingOutput"), "manual apply/save buttons removed");
    for (const auto& pair : {qMakePair(source, "restartCapturePreview")}) {
        auto* action = findItem(workspace, pair.second);
        const qreal beforeY = action->mapToItem(pair.first, QPointF()).y();
        auto* panelScroll = findItem(pair.first, pair.first->objectName() + "Scroll");
        auto* scrolling = panelScroll->property("contentItem").value<QObject*>();
        scrolling->setProperty("contentY", qMax(0.0, scrolling->property("contentHeight").toReal() - scrolling->property("height").toReal())); settle();
        check(qAbs(action->mapToItem(pair.first, QPointF()).y() - beforeY) < 0.1
            && beforeY + action->height() <= pair.first->height(), "footer action remains fixed while settings scroll");
        scrolling->setProperty("contentY", 0);
    }
    window->setHeight(780); settle();
    window->setWidth(850); window->setHeight(680); settle();
    QMetaObject::invokeMethod(bitratePopup, "open"); settle();
    check(popupFits(), "bitrate popup repositions within narrow and shorter window");
    QMetaObject::invokeMethod(bitratePopup, "close"); settle();
    check(source->y() == center->y() && center->y() == output->y()
        && source->x() < center->x() && center->x() < output->x(), "medium layout keeps tools beside preview");
    check(toolbar->width() == preview->width() && toolbar->y() >= preview->height()
        && findItem(workspace, "startLocalRecording")->mapToItem(output, QPointF(0,0)).x() >= 0,
        "independent mixer stays below preview while output actions live in the right panel");
    check(findItem(toolbar,"recordingAudioTools")->width() >= 446, "narrow workspace never collapses the mixer viewport");
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/medium.png");
    window->setWidth(560); settle();
    check(source->y() == center->y() && center->y() == output->y()
        && source->x() < center->x() && center->x() < output->x()
        && source->width() == workspace->property("sourcePanelWidth").toReal()
        && output->width() == workspace->property("outputPanelWidth").toReal()
        && preview->width() >= 480, "small layout preserves three columns and usable widths");
    auto* scroll = findItem(workspace, "recordingWorkspaceScroll");
    auto* studio = findItem(workspace, "recordingStudioGrid");
    check(scroll && studio && studio->width() > scroll->width(), "narrow workspace scrolls horizontally");
    auto* flickable = scroll->property("contentItem").value<QObject*>();
    flickable->setProperty("contentX", studio->width() - scroll->width()); settle();
    check(flickable->property("contentX").toReal() > 0
        && output->mapToItem(workspace, QPointF(output->width(), 0)).x() <= workspace->width(), "right toolbar reachable by scrolling");
    window->setWidth(1320); settle();
    QVariant result;
    QVariant frameRateResult;
    check(QMetaObject::invokeMethod(window, "runFrameRateChecks", Q_RETURN_ARG(QVariant, frameRateResult)) && frameRateResult.toInt() == 9, "cross-panel frame-rate save/apply/start checks");
    check(QMetaObject::invokeMethod(window, "runChecks", Q_RETURN_ARG(QVariant, result)), "page interaction checks execute");
    check(result.toInt() == 24, "24 source/save/start/rollback/reconnect checks");
    auto* fake = window->property("fake").value<QObject*>();
    check(fake, "fixture controller");
    check(fake->property("audioMonitoringEnabled").toBool(),"visible workspace requests idle audio capture");
    workspace->setVisible(false);settle();
    check(!fake->property("audioMonitoringEnabled").toBool(),"hidden workspace releases idle monitoring request");
    workspace->setVisible(true);settle();
    check(fake->property("audioMonitoringEnabled").toBool(),"returning to workspace restores idle monitoring request");

    auto* microphone=findItem(toolbar,"recordingMicrophone");
    auto* microphoneVolume=findItem(toolbar,"recordingMicrophoneVolume");
    auto* systemVolume=findItem(toolbar,"recordingSystemVolume");
    check(microphone && microphoneVolume && systemVolume && !microphoneVolume->isEnabled(), "audio volume and microphone UI exists with microphone initially off");
    auto* systemToggle=findItem(toolbar,"recordingAudio");
    if (!systemToggle->property("checked").toBool()) {
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,systemToggle->mapToScene(QPointF(systemToggle->width()/2,systemToggle->height()/2)).toPoint());
        settle();settle();
    }
    fake->setProperty("audioLevels",QVariantMap{{"active",true},{"systemPeak",.3},{"microphonePeak",0.0}});settle();
    auto* idleSystemMeter=findItem(toolbar,"recordingSystemLevelMeter");
    check(!fake->property("recordingBusy").toBool() && idleSystemMeter->property("available").toBool() && idleSystemMeter->property("targetLevel").toDouble()>.7,
          "idle system levels display before any recording or streaming starts");
    fake->setProperty("audioLevels",QVariantMap{{"active",false}});settle();
    const auto originalOptions=fake->property("options").value<QJSValue>().toVariant();
    const int originalSaves=fake->property("saves").toInt();
    QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,microphone->mapToScene(QPointF(microphone->width()/2,microphone->height()/2)).toPoint());
    settle();
    check(toolbar->property("microphoneEnabled").toBool() && microphoneVolume->isEnabled(), "microphone UI toggle enables its volume control");
    microphoneVolume->forceActiveFocus();QTest::keyClick(window,Qt::Key_Left);
    systemVolume->forceActiveFocus();QTest::keyClick(window,Qt::Key_Left);settle();
    check(toolbar->property("microphoneVolume").toDouble()<100 && toolbar->property("systemVolume").toDouble()<100, "audio sliders edit their UI values");
    check(fake->property("saves").toInt()==originalSaves && fake->property("options").value<QJSValue>().toVariant()==originalOptions, "UI-only audio controls do not change backend recording settings");
    check(fake->property("previewAudio").value<QJSValue>().toVariant().toMap().value("microphoneEnabled").toBool(), "microphone toggle previews actual live audio settings");
    QVariant audioSaved;
    check(QMetaObject::invokeMethod(workspace,"saveLiveAudio",Q_RETURN_ARG(QVariant,audioSaved)) && audioSaved.toBool(), "audio auto-save flush succeeds");
    const auto savedLive=fake->property("liveOptions").value<QJSValue>().toVariant().toMap();
    check(savedLive.value("microphoneEnabled").toBool() && savedLive.value("microphoneVolume").toInt()<100 && savedLive.value("systemVolume").toInt()<100,
          "independent microphone and system gain persist in live settings");
    auto stableGeometry=[&] {
        auto* controls=findItem(toolbar,"recordingControlRow");
        auto* status=findItem(toolbar,"recordingStatusRow");
        check(toolbar->height()<=132 && controls && status && status->y()>=controls->y()+controls->height(), "mixer has a compact audio row, heading and status row");
        auto centerY=[&](const char* name) { auto* item=findItem(toolbar,name); return item->mapToItem(toolbar,QPointF(0,item->height()/2)).y(); };
        check(qAbs(centerY("recordingSystemAudioTool")-centerY("recordingMicrophoneTool"))<1, "independent audio tools stay on one line at all widths");
        check(qAbs(centerY("recordingStatus")-centerY("recordingOutputLocation"))<2, "status and output path share one footer line");

        auto snapshot=[&] { return QVariantList{preview->y(),preview->height(),toolbar->y(),toolbar->height(),findItem(workspace,"startLocalRecording")->width(),findItem(workspace,"stopLocalRecording")->width()}; };
        check(previewBadge->mapToItem(preview, QPointF(previewBadge->width(), 0)).x() <= preview->width() - 6
            && previewTools->x() >= 4 && previewTools->x() + previewTools->width() <= preview->width() - 4,
            "preview badge and floating tools remain within narrow frame");
        const auto before=snapshot();
        const auto previousMessage=pageState->property("message");
        for (const auto* phase : {"Preparing","Recording","Stopping","Error","Idle"}) {
            fake->setProperty("recordingState",phase);
            fake->setProperty("recordingBusy",QString(phase)!="Idle" && QString(phase)!="Error");
            fake->setProperty("recordingOutputPath",QString(phase)=="Preparing"?QString():QString("C:/Videos/very-long-folder/recording-with-long-file-name.mp4"));
            pageState->setProperty("message",QString::fromUtf8("很长的状态提示不会挤压预览画面，也不会因为文件路径出现而改变占位。"));
            settle();
            check(snapshot()==before,"preview and toolbar geometry stays fixed across recording states and output-path appearance");
        }
        fake->setProperty("recordingOutputPath",QString());
        pageState->setProperty("message",previousMessage);settle();
    };
    stableGeometry();
    window->setWidth(850);settle();stableGeometry();
    auto* audioTools=findItem(toolbar,"recordingAudioTools");
    auto* audioFlick=audioTools->property("contentItem").value<QObject*>();
    check(findItem(toolbar,"recordingSystemAudioTool")->width()>=220 && findItem(toolbar,"recordingMicrophoneTool")->width()>=220 && audioTools->width()>=446, "both audio modules retain usable width in narrow windows");
    audioFlick->setProperty("contentX",audioFlick->property("contentWidth").toReal()-audioFlick->property("width").toReal());settle();
    check(microphone->mapToItem(audioTools,QPointF(microphone->width(),0)).x()<=audioTools->width(), "microphone tools remain reachable by horizontal scrolling");
    window->setWidth(1320);settle();
    fake->setProperty("recordingBusy", true); fake->setProperty("recordingState", "Recording"); settle();
    check(findItem(workspace, "recordingStatus")->property("text").toString() == QString::fromUtf8("正在录制"), "recording status overrides old settings messages");
    check(!findItem(workspace, "startLocalRecording")->isEnabled() && !findItem(workspace, "recordingDirectory")->isEnabled()
        && findItem(workspace, "recordingAudio")->isEnabled() && !findItem(workspace, "restartCapturePreview")->isEnabled(), "actual controls locked while busy");
    check(microphone->isEnabled() && microphoneVolume->isEnabled() && systemVolume->isEnabled(), "shared audio controls remain active during local recording");
    microphoneVolume->forceActiveFocus();QTest::keyClick(window,Qt::Key_Left);settle();
    check(QMetaObject::invokeMethod(workspace,"saveLiveAudio",Q_RETURN_ARG(QVariant,audioSaved)) && audioSaved.toBool(),"local recording permits audio settings save");
    check(fake->property("recordingState")=="Recording", "changing local mix does not stop or restart recording");
    auto* systemMeter=findItem(toolbar,"recordingSystemLevelMeter");
    auto* microphoneMeter=findItem(toolbar,"recordingMicrophoneLevelMeter");
    check(systemMeter && microphoneMeter && systemMeter->height()==4 && microphoneMeter->height()==4,"compact independent level meters added below gain sliders");
    fake->setProperty("audioLevels",QVariantMap{{"active",true},{"systemPeak",.5},{"microphonePeak",.95},{"systemFullScale",false},{"microphoneFullScale",true}});settle();
    check(systemMeter->property("available").toBool() && systemMeter->property("targetLevel").toDouble()>.8 && microphoneMeter->property("fullScale").toBool(),"recording meters show live measured peaks and overload warning");
    check(microphoneMeter->property("description").toString().contains("dBFS"),"level tooltip exposes measured decibels");
    QTest::qWait(80);settle();window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/meters-active.png");
    fake->setProperty("audioLevels",QVariantMap{{"active",false}});settle();
    check(systemMeter->property("targetLevel").toDouble()==0 && !microphoneMeter->property("available").toBool(),"inactive session clears meters");
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/recording.png");
    auto* stop = findItem(workspace, "stopLocalRecording");
    check(stop->isEnabled() && QMetaObject::invokeMethod(stop, "clicked"), "actual stop button submits request");
    settle(); check(!stop->isEnabled() && !findItem(workspace, "recordingFilePrefix")->isEnabled(), "stopping keeps controls locked");
    fake->setProperty("recordingBusy", false); fake->setProperty("recordingState", "Idle"); settle();
    auto* widthField = findItem(workspace, "recordingWidth");
    widthField->setProperty("text", ""); check(QMetaObject::invokeMethod(widthField, "textEdited"), "dimension field editing");
    settle(); check(widthField->property("text").toString().isEmpty(), "empty dimension remains editable for backend validation");
    widthField->setProperty("text", "1920"); QMetaObject::invokeMethod(widthField, "textEdited");
    auto* heightField = findItem(workspace, "recordingHeight"); heightField->setProperty("text", "1080"); QMetaObject::invokeMethod(heightField, "textEdited");
    check(QMetaObject::invokeMethod(heightField, "editingFinished"), "field completion requests automatic save");
    settle(); settle();
    check(fake->property("options").value<QJSValue>().toVariant().toMap().value("width").toInt() == 1920, "actual field edits aggregate into saved settings");
    for (int step = 0; step <= 14; ++step) {
        QVariant outcome;
        check(QMetaObject::invokeMethod(window, "runAutoStep", Q_RETURN_ARG(QVariant, outcome), Q_ARG(QVariant, step)) && outcome.toBool(), "automatic save/apply asynchronous workflow");
        settle(); settle();
    }
    workspace->setProperty("customOutputFps", true); settle();
    auto* fpsField = findItem(workspace, "recordingFps");
    window->requestActivate(); fpsField->forceActiveFocus(); settle();
    QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
    QTest::keyClick(window, Qt::Key_2); QTest::keyClick(window, Qt::Key_5);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, fpsField->mapToScene(QPointF(12, 12)).toPoint());
    check(fpsField->hasActiveFocus(), "clicking inside FPS field keeps editing focus");
    const auto previewClick=preview->mapToScene(QPointF(preview->width()/2, preview->height()/2)).toPoint();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, previewClick); QTest::qWait(40);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, previewClick);
    settle(); settle();
    check(!fpsField->hasActiveFocus() && fake->property("options").value<QJSValue>().toVariant().toMap().value("fps").toInt() == 25, "clicking preview clears FPS focus and automatically saves without Enter");
    check(!pageState->property("outputDirty").toBool() && findItem(workspace, "recordingOutputPanelStatus")->property("text").toString() == QString::fromUtf8("已保存"), "focus-loss save clears pending status after real keyboard editing");
    auto typeFps = [&](const QString& digits) {
        fpsField->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
        for (const auto digit : digits) QTest::keyClick(window, static_cast<Qt::Key>(Qt::Key_0 + digit.digitValue()));
    };
    const int savesBeforeTyping=fake->property("saves").toInt();
    typeFps("24"); QTest::qWait(1000);
    check(fake->property("saves").toInt()==savesBeforeTyping, "typing does not save before idle deadline");
    typeFps("26"); QTest::qWait(1400);
    check(fake->property("saves").toInt()==savesBeforeTyping, "new keystrokes restart two-second idle deadline");
    QTest::qWait(1000); settle();
    check(fpsField->hasActiveFocus() && fake->property("options").value<QJSValue>().toVariant().toMap().value("fps").toInt()==26 && !pageState->property("outputDirty").toBool(), "two seconds idle saves final value and status without Enter or focus loss");
    typeFps("27"); QTest::keyClick(window, Qt::Key_Return); settle(); settle();
    check(fake->property("options").value<QJSValue>().toVariant().toMap().value("fps").toInt()==27, "Enter still commits immediately");
    auto* fpsPreset = findItem(workspace, "outputFpsChoicesOption0");
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, fpsPreset->mapToScene(QPointF(fpsPreset->width()/2, fpsPreset->height()/2)).toPoint());
    settle(); settle();
    check(fake->property("options").value<QJSValue>().toVariant().toMap().value("fps").toInt() == 30, "outside-click observer preserves preset button clicks");
    {
        auto* button=findItem(workspace,"recordingPreviewHeaderInfo");
        const bool previous=button->property("checked").toBool();
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,button->mapToScene(QPointF(button->width()/2,button->height()/2)).toPoint());
        settle();
        check(button->property("checked").toBool()!=previous,"header info button toggles through actual mouse input");
    }
    QVariant requestChecks;
    check(QMetaObject::invokeMethod(window, "runRequestIdChecks", Q_RETURN_ARG(QVariant, requestChecks)) && requestChecks.toInt() == 8,
        "request IDs, stale replies, pending drafts and preview busy use production page state");
    engine.load(QUrl::fromLocalFile(QStringLiteral(APP_ROOT) + "/tests/recording-workspace/PageFixture.qml"));
    settle(); check(engine.rootObjects().size() == 2, "production page and preview load with fake service");
    auto* client = engine.singletonInstance<QObject*>(fakeClientId);
    check(client && client->property("attempts").toInt() > 0 && client->property("previewStarts").toInt() == 0, "failed render attachment does not start preview");
    client->setProperty("attachAllowed", true);
    QElapsedTimer retry; retry.start(); while (client->property("previewStarts").toInt() == 0 && retry.elapsed() < 2000) { QCoreApplication::processEvents(); QThread::msleep(2); }
    check(client->property("previewStarts").toInt() == 1, "retry starts preview after successful attachment");
    check(client->property("scale").toInt() == 2, "page initializes native Fill for the composition editor");
    auto* pageWindow = qobject_cast<QQuickWindow*>(engine.rootObjects().last());
    auto* pageViewport = findItem(pageWindow->contentItem(), "recordingViewport");
    auto* previewHost = client->property("previewHost").value<QQuickItem*>();
    check(pageViewport && previewHost, "production preview renderer stays attached to the viewport host");
    theme.insert("Oran7ScreenCapture", QVariantMap{{"ScaleMode", "Fit"}, {"workspaceSkin", "violet"}});
    settle();
    check(client->property("scale").toInt() == 2 && previewHost->size() == pageViewport->size() &&
              qAbs(pageViewport->width() / pageViewport->height() - 16.0 / 9) < 0.01,
          "snowfield Fit supplies aspect-correct host geometry and renders edge-to-edge");
    auto* pageTools = findItem(pageWindow->contentItem(), "recordingSelectionTools");
    auto* navigation = findItem(pageWindow->contentItem(), "fixtureNavigation");
    check(pageTools && navigation, "production preview and external navigation are mounted");
    QTest::mouseClick(pageWindow, Qt::LeftButton, Qt::NoModifier,
                      pageViewport->mapToScene(QPointF(pageViewport->width() / 2, pageViewport->height() / 2)).toPoint());
    check(pageTools->isVisible(), "clicking the production preview selects it");
    QTest::mouseClick(pageWindow, Qt::LeftButton, Qt::NoModifier,
                      navigation->mapToScene(QPointF(navigation->width() / 2, navigation->height() / 2)).toPoint());
    check(!pageTools->isVisible() && pageWindow->property("navigationClicks").toInt() == 1,
          "window-wide deselection preserves the navigation button click");
    const QVariant portraitFrame = QVariantMap{{"sourceTag", "screen"}, {"srcWidth", 1080}, {"srcHeight", 1920}};
    check(QMetaObject::invokeMethod(client, "videoRenderInfoUpdated", Q_ARG(QVariant, portraitFrame)),
          "preview receives updated frame size");
    settle();
    check(previewHost->size() == pageViewport->size() && qAbs(pageViewport->width() / pageViewport->height() - 9.0 / 16) < 0.01,
          "actual frame aspect replaces the monitor-size hint without introducing inner bars");
    theme.insert("Oran7ScreenCapture", QVariantMap{{"ScaleMode", "Fit"}, {"workspaceSkin", "default"}});
    settle();
    check(client->property("scale").toInt() == 2 && previewHost->size() == pageViewport->size() &&
              qAbs(pageViewport->width() / pageViewport->height() - 9.0 / 16) < 0.01,
          "default skin keeps shared Fit geometry and native Fill without replacing the preview host");
    const auto originalPanelColor = source->property("color");
    const auto originalTextColor = workspace->property("textColor");
    auto* settings = engine.singletonInstance<QObject*>(qmlTypeId("Oran7MainUiSetting", 1, 0, "Oran7MainUiSetting"));
    check(settings, "dropdown style singleton available");
    settings->setProperty("itemHeight", 48);
    settings->setProperty("textPixelSize", 24);
    settings->setProperty("fontFamily", "Times New Roman");
    theme.insert("Oran7MainGUI", QVariantMap{{"themeColor", "#00ff00"}, {"isDarkMode", false}});
    theme.insert("isDark",false);
    settle();
    check(source->property("color") == originalPanelColor && toolbar->property("color") == originalPanelColor
        && workspace->property("textColor") == originalTextColor && workspace->property("textPixelSize").toInt() == 16,
        "dropdown palette/fonts do not leak into workspace");
    workspace->setProperty("panelColor", QColor("#224466"));
    workspace->setProperty("textColor", QColor("#fff0aa"));
    workspace->setProperty("accentColor", QColor("#ff8844"));
    settle();
    check(source->property("color").value<QColor>() == QColor("#224466")
        && toolbar->property("color").value<QColor>() == QColor("#224466")
        && toolbar->property("textColor").value<QColor>() == QColor("#fff0aa"), "workspace properties style panels and toolbar");
    auto* audio = findItem(toolbar, "recordingAudio");
    check(audio->property("onColor").value<QColor>() == QColor("#ff8844")
        && audio->height() == 20, "reused switch uses injected workspace colors and sizing");
    settle();
    QVariant liveChecks;
    check(QMetaObject::invokeMethod(window,"runLiveChecks",Q_RETURN_ARG(QVariant,liveChecks)) && liveChecks.toInt()==3,
          "live auto-save is separate from recording and failed saves preserve drafts");
    auto* liveButton=findItem(workspace,"startLiveStreaming");
    check(liveButton && findItem(workspace,"liveServer") && findItem(workspace,"liveStreamKey"),"live controls instantiated");
    check(findItem(workspace,"liveStreamKey")->property("echoMode").toInt()==2,"stream key masked by default");
    QMetaObject::invokeMethod(liveButton,"clicked");settle();
    auto* liveSession=captureFixture->property("liveSession").value<QObject*>();
    check(liveSession && liveSession->property("busy").toBool() && findItem(workspace,"startLocalRecording")->isEnabled(),
          "live button connects while local recording remains independently available");
    check(!findItem(workspace,"liveServer")->isEnabled(),"live address locked while connecting");
    liveSession->setProperty("state","Streaming");settle();
    check(microphone->isEnabled() && systemToggle->isEnabled(), "audio toggles stay usable during live streaming");
    if(!systemToggle->property("checked").toBool())QMetaObject::invokeMethod(systemToggle,"switchToggleChanged",Q_ARG(bool,true));
    if(!microphone->property("checked").toBool())QMetaObject::invokeMethod(microphone,"switchToggleChanged",Q_ARG(bool,true));
    settle();
    check(microphoneVolume->isEnabled() && systemVolume->isEnabled(), "enabled audio sources expose live gain sliders");
    systemVolume->forceActiveFocus();QTest::keyClick(window,Qt::Key_Left);settle();
    check(QMetaObject::invokeMethod(workspace,"saveLiveAudio",Q_RETURN_ARG(QVariant,audioSaved)) && audioSaved.toBool(), "live audio settings save without stopping stream");
    check(liveSession->property("state")=="Streaming" && liveSession->property("busy").toBool(),"changing live gain does not restart session");
    auto* recordButton=findItem(workspace,"startLocalRecording");
    check(recordButton->isEnabled() && QMetaObject::invokeMethod(recordButton,"clicked"),"recording can start from toolbar while RTMP is streaming");settle();
    check(fake->property("recordingBusy").toBool() && liveSession->property("state")=="Streaming","starting recording preserves live session");
    fake->setProperty("recordingState","Recording");settle();
    check(findItem(workspace,"recordingStatus")->property("text").toString().contains(QString::fromUtf8("正在录制")) &&
          findItem(workspace,"recordingStatus")->property("text").toString().contains(QString::fromUtf8("推流中")),"status row displays both active sessions");
    QMetaObject::invokeMethod(findItem(workspace,"stopLocalRecording"),"clicked");settle();
    check(liveSession->property("state")=="Streaming","local stop button does not stop live");
    fake->setProperty("recordingState","Idle");fake->setProperty("recordingBusy",false);settle();
    QMetaObject::invokeMethod(liveButton,"clicked");settle();
    check(liveSession->property("state")=="Stopping" && !liveButton->isEnabled(),"second click cancels connection then prevents duplicate stop");
    liveSession->setProperty("state","Idle");liveSession->setProperty("busy",false);settle();
    fake->setProperty("recordingState","Recording");fake->setProperty("recordingBusy",true);settle();
    check(liveButton->isEnabled() && findItem(workspace,"liveServer")->isEnabled(),"live controls stay available during recording");
    QMetaObject::invokeMethod(liveButton,"clicked");settle();
    check(liveSession->property("state")=="Connecting" && fake->property("recordingState")=="Recording","live can connect without restarting active recording");
    QMetaObject::invokeMethod(liveButton,"clicked");settle();
    check(fake->property("recordingState")=="Recording","cancel live connection preserves recording");
    liveSession->setProperty("state","Idle");liveSession->setProperty("busy",false);
    fake->setProperty("recordingState","Idle");fake->setProperty("recordingBusy",false);settle();
    check(QMetaObject::invokeMethod(workspace,"editLiveAudio",Q_ARG(QVariant,QString("systemVolume")),Q_ARG(QVariant,23)),"edit final unsaved audio value");
    check(QMetaObject::invokeMethod(workspace,"editLive",Q_ARG(QVariant,QString("bitrateKbps")),Q_ARG(QVariant,8000)),"edit final unsaved RTMP value");
    check(QMetaObject::invokeMethod(pageState,"editOutput",Q_ARG(QVariant,QString("filePrefix")),Q_ARG(QVariant,QString::fromUtf8("退出前最终输入")),Q_ARG(QVariant,true)),"edit deferred final recording value");
    const int appliesBeforeClose=fake->property("captureApplies").toInt();
    window->close(); // 不等两秒，也不再推进事件循环。
    auto persistedLive=fake->property("liveOptions").value<QJSValue>().toVariant().toMap();
    auto persistedOutput=fake->property("options").value<QJSValue>().toVariant().toMap();
    check(persistedLive.value("systemVolume").toInt()==23 && persistedLive.value("bitrateKbps").toInt()==8000
          && persistedOutput.value("filePrefix").toString()==QString::fromUtf8("退出前最终输入"),"window close synchronously flushes audio, live and deferred file settings");
    check(fake->property("captureApplies").toInt()==appliesBeforeClose,"exit flush never restarts or applies capture");
    const int savesBeforeRepeat=fake->property("saves").toInt();
    QMetaObject::invokeMethod(fake,"settingsFlushRequested");
    check(fake->property("saves").toInt()==savesBeforeRepeat,"controller fallback flush is idempotent after window close");
    QMetaObject::invokeMethod(workspace,"editLiveAudio",Q_ARG(QVariant,QString("microphoneVolume")),Q_ARG(QVariant,37));
    QMetaObject::invokeMethod(fake,"settingsFlushRequested");
    check(fake->property("liveOptions").value<QJSValue>().toVariant().toMap().value("microphoneVolume").toInt()==37,
          "backend quit notification flushes settings without a window closing event");
    fake->setProperty("reject",true);
    QMetaObject::invokeMethod(workspace,"editLive",Q_ARG(QVariant,QString("bitrateKbps")),Q_ARG(QVariant,9000));
    QMetaObject::invokeMethod(fake,"settingsFlushRequested");
    check(fake->property("liveOptions").value<QJSValue>().toVariant().toMap().value("bitrateKbps").toInt()==8000 && workspace->property("liveDirty").toBool(),
          "failed exit save preserves last committed values and leaves unsaved draft identifiable");
    fake->setProperty("reject",false);
    QMetaObject::invokeMethod(fake,"settingsFlushRequested");
    check(!workspace->property("liveDirty").toBool(),"failed exit flush remains retryable");
    checkPersistentScenes(engine);
    check(warnings.isEmpty(), "no QML warnings or binding loops");
    std::printf("PASS: %d layout/runtime checks + %d recording workflow checks\n", checks, result.toInt());
}
