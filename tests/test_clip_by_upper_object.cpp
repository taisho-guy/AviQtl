#include "effect_registry.hpp"
#include "project_serializer.hpp"
#include "project_service.hpp"
#include "selection_service.hpp"
#include "timeline_service.hpp"
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace AviQtl::UI;

class TestClipByUpperObject : public QObject {
    Q_OBJECT

  private slots:
    void defaultValueIsFalse() {
        SelectionService selection;
        TimelineService timeline(&selection);

        ClipData clip;
        clip.id = 1;
        clip.sceneId = timeline.currentSceneId();
        clip.type = QStringLiteral("image");
        clip.startFrame = 0;
        clip.durationFrames = 10;
        clip.layer = 1;
        timeline.addClipDirectInternal(clip);

        QCOMPARE(timeline.clipByUpperObject(1), false);
    }

    void toggleSupportsUndoRedo() {
        SelectionService selection;
        TimelineService timeline(&selection);

        ClipData clip;
        clip.id = 1;
        clip.sceneId = timeline.currentSceneId();
        clip.type = QStringLiteral("image");
        clip.startFrame = 0;
        clip.durationFrames = 10;
        clip.layer = 1;
        timeline.addClipDirectInternal(clip);

        QSignalSpy changedSpy(&timeline, &TimelineService::clipsChanged);
        timeline.setClipByUpperObject(1, true);
        QCOMPARE(timeline.clipByUpperObject(1), true);
        QVERIFY(changedSpy.count() >= 1);

        timeline.undo();
        QCOMPARE(timeline.clipByUpperObject(1), false);

        timeline.redo();
        QCOMPARE(timeline.clipByUpperObject(1), true);
    }

    void serializerRoundTripsFlag() {
        SelectionService selection;
        TimelineService timeline(&selection);
        ProjectService project;

        ClipData clip;
        clip.id = 1;
        clip.sceneId = timeline.currentSceneId();
        clip.type = QStringLiteral("image");
        clip.startFrame = 0;
        clip.durationFrames = 10;
        clip.layer = 1;
        clip.clipByUpperObject = true;
        timeline.addClipDirectInternal(clip);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("project.aviqtl"));
        QString error;
        QVERIFY2(AviQtl::Core::ProjectSerializer::save(path, &timeline, &project, &error), qPrintable(error));

        SelectionService loadedSelection;
        TimelineService loadedTimeline(&loadedSelection);
        ProjectService loadedProject;
        QVERIFY2(AviQtl::Core::ProjectSerializer::load(path, &loadedTimeline, &loadedProject, &error), qPrintable(error));

        QCOMPARE(loadedTimeline.clipByUpperObject(1), true);
    }

    void serializerSkipsMissingEffects() {
        AviQtl::Core::EffectMetadata meta;
        meta.id = QStringLiteral("compat.present.effect");
        meta.name = QStringLiteral("Present Effect");
        meta.kind = QStringLiteral("effect");
        meta.categories = {QStringLiteral("Compat")};
        meta.qmlSource = QStringLiteral("PresentEffect.qml");
        AviQtl::Core::EffectRegistry::instance().registerEffect(meta);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("project-with-missing-effect.aviqtl"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"JSON({
  "settings": {
    "width": 1920,
    "height": 1080,
    "fps": 60,
    "sampleRate": 48000
  },
  "scenes": [
    {
      "id": 0,
      "name": "Root",
      "width": 1920,
      "height": 1080,
      "fps": 60,
      "start": 0,
      "duration": 100
    }
  ],
  "clips": [
    {
      "id": 1,
      "sceneId": 0,
      "type": "image",
      "start": 0,
      "duration": 100,
      "layer": 0,
      "effects": [
        {
          "id": "compat.present.effect",
          "name": "Present Effect",
          "enabled": true,
          "params": {}
        },
        {
          "id": "compat.removed.effect",
          "name": "Removed Effect",
          "enabled": true,
          "params": {
            "strength": 42
          }
        }
      ]
    }
  ]
})JSON");
        file.close();

        SelectionService loadedSelection;
        TimelineService loadedTimeline(&loadedSelection);
        ProjectService loadedProject;
        QString error;
        QVERIFY2(AviQtl::Core::ProjectSerializer::load(path, &loadedTimeline, &loadedProject, &error), qPrintable(error));

        QCOMPARE(loadedTimeline.clips().size(), 1);
        QCOMPARE(loadedTimeline.clips().first().effects.size(), 1);
        QCOMPARE(loadedTimeline.clips().first().effects.first()->id(), QStringLiteral("compat.present.effect"));
    }
};

QTEST_MAIN(TestClipByUpperObject)
#include "test_clip_by_upper_object.moc"
