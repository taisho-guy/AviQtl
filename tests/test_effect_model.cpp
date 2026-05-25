#include "effect_model.hpp"
#include <QSignalSpy>
#include <QTest>

using namespace AviQtl::UI;

class TestEffectModel : public QObject {
    Q_OBJECT

  private slots:
    void constructorInitializesDefaults() {
        EffectModel m(QStringLiteral("test.id"), QStringLiteral("Test"), QStringLiteral("effect"), {QStringLiteral("VFX")}, {{"opacity", QVariant(100)}, {"pos.x", QVariant(0)}});
        QCOMPARE(m.id(), QStringLiteral("test.id"));
        QCOMPARE(m.name(), QStringLiteral("Test"));
        QCOMPARE(m.kind(), QStringLiteral("effect"));
        QCOMPARE(m.categories().size(), 1);
        QVERIFY(m.isEnabled());
        QVERIFY(m.keyframeTracks().contains(QStringLiteral("opacity")));
        QVERIFY(m.keyframeTracks().contains(QStringLiteral("pos.x")));
        QCOMPARE(m.params().value(QStringLiteral("opacity")).toInt(), 100);
    }

    void cloneCopiesFields() {
        EffectModel original(QStringLiteral("id"), QStringLiteral("Name"), QStringLiteral("object"), {QStringLiteral("3D")}, {{"scale", QVariant(1.5)}});
        original.setEnabled(false);
        auto copy = std::unique_ptr<EffectModel>(original.clone());
        QCOMPARE(copy->id(), QStringLiteral("id"));
        QCOMPARE(copy->isEnabled(), false);
        QCOMPARE(copy->params().value(QStringLiteral("scale")).toDouble(), 1.5);
    }

    void setEnabledSignal() {
        EffectModel m(QStringLiteral("x"), QStringLiteral("Y"), QStringLiteral("effect"), QStringList());
        QSignalSpy spy(&m, &EffectModel::enabledChanged);
        m.setEnabled(true); // no change
        QCOMPARE(spy.count(), 0);
        m.setEnabled(false);
        QCOMPARE(spy.count(), 1);
    }

    void setParamUpdatesTrackStartValue() {
        EffectModel m(QStringLiteral("x"), QStringLiteral("Y"), QStringLiteral("effect"), QStringList(), {{"opacity", QVariant(0)}});
        QSignalSpy spy(&m, &EffectModel::paramsChanged);
        QSignalSpy kfSpy(&m, &EffectModel::keyframeTracksChanged);

        m.setParam(QStringLiteral("opacity"), QVariant(255));
        QCOMPARE(m.params().value(QStringLiteral("opacity")).toInt(), 255);
        QCOMPARE(spy.count(), 1);

        // start keyframe should also have been updated
        QVERIFY(m.keyframeTracks().contains(QStringLiteral("opacity")));
    }

    void evaluatedParamNoKeyframeReturnsFallback() {
        EffectModel m(QStringLiteral("x"), QStringLiteral("Y"), QStringLiteral("effect"), QStringList(), {{"volume", QVariant(0.8)}});
        QVariant val = m.evaluatedParam(QStringLiteral("volume"), 0);
        QCOMPARE(val.toDouble(), 0.8);
        val = m.evaluatedParam(QStringLiteral("volume"), 100);
        QCOMPARE(val.toDouble(), 0.8);
    }

    void availableEasings() {
        EffectModel m(QStringLiteral("x"), QStringLiteral("Y"), QStringLiteral("effect"), QStringList());
        QStringList easings = m.availableEasings();
        QVERIFY(!easings.isEmpty());
        QVERIFY(easings.contains(QStringLiteral("linear")));
        QVERIFY(easings.contains(QStringLiteral("ease_out_bounce")));
        QVERIFY(easings.contains(QStringLiteral("custom")));
        QVERIFY(easings.contains(QStringLiteral("random")));
    }

    void isEndpointFrame() {
        EffectModel m(QStringLiteral("x"), QStringLiteral("Y"), QStringLiteral("effect"), QStringList(), {{"pos", QVariant(0)}});
        QVERIFY(m.isEndpointFrame(QStringLiteral("pos"), 0));
        QVERIFY(!m.isEndpointFrame(QStringLiteral("pos"), 10));
    }

    void moveKeyframePreservesState() {
        EffectModel m(QStringLiteral("x"), QStringLiteral("Y"), QStringLiteral("effect"), QStringList(), {{"pos", QVariant(0)}});
        QVariantList points;
        points << 0.2 << 0.3 << 0.7 << 0.8 << 1.0 << 1.0;
        QVariantMap modeParams;
        modeParams[QStringLiteral("stepFrames")] = 3;
        m.setKeyframe(QStringLiteral("pos"), 10, QVariant(200), {{"interp", QStringLiteral("custom")}, {"points", points}, {"modeParams", modeParams}});

        QVERIFY(m.moveKeyframe(QStringLiteral("pos"), 10, 8));
        const QVariantList track = m.keyframeListForUi(QStringLiteral("pos"));
        QCOMPARE(track.size(), 2);
        QCOMPARE(track[1].toMap().value(QStringLiteral("frame")).toInt(), 8);
        QCOMPARE(track[1].toMap().value(QStringLiteral("value")).toInt(), 200);
        QCOMPARE(track[1].toMap().value(QStringLiteral("interp")).toString(), QStringLiteral("custom"));
        QCOMPARE(track[1].toMap().value(QStringLiteral("points")).toList().size(), 6);
        QCOMPARE(track[1].toMap().value(QStringLiteral("modeParams")).toMap().value(QStringLiteral("stepFrames")).toInt(), 3);
        QCOMPARE(m.evaluatedParam(QStringLiteral("pos"), 8).toInt(), 200);
    }

    void splitKeyframeCanPreserveInterpolation() {
        EffectModel m(QStringLiteral("x"), QStringLiteral("Y"), QStringLiteral("effect"), QStringList(), {{"pos", QVariant(0)}});
        m.setKeyframe(QStringLiteral("pos"), 0, QVariant(0), {{"interp", QStringLiteral("linear")}});
        m.setKeyframe(QStringLiteral("pos"), 10, QVariant(200), {{"interp", QStringLiteral("none")}});

        const QVariant midValue = m.evaluatedParam(QStringLiteral("pos"), 5);
        m.setKeyframe(QStringLiteral("pos"), 5, midValue, {{"interp", QStringLiteral("linear")}});

        const QVariantList track = m.keyframeListForUi(QStringLiteral("pos"));
        QCOMPARE(track.size(), 3);
        QCOMPARE(track[1].toMap().value(QStringLiteral("frame")).toInt(), 5);
        QCOMPARE(track[1].toMap().value(QStringLiteral("value")).toInt(), 100);
        QCOMPARE(track[1].toMap().value(QStringLiteral("interp")).toString(), QStringLiteral("linear"));
        QCOMPARE(m.evaluatedParam(QStringLiteral("pos"), 3).toDouble(), 60.0);
        QCOMPARE(m.evaluatedParam(QStringLiteral("pos"), 8).toDouble(), 160.0);
    }

    void keyframeListForUiDoesNotInvalidateEvaluation() {
        EffectModel m(QStringLiteral("x"), QStringLiteral("Y"), QStringLiteral("effect"), QStringList(), {{"pos", QVariant(0)}});
        m.setKeyframe(QStringLiteral("pos"), 0, QVariant(0), {{"interp", QStringLiteral("linear")}});
        m.setKeyframe(QStringLiteral("pos"), 10, QVariant(100), {{"interp", QStringLiteral("none")}});

        QCOMPARE(m.evaluatedParam(QStringLiteral("pos"), 5).toDouble(), 50.0);
        for (int i = 0; i < 5; ++i) {
            const QVariantList track = m.keyframeListForUi(QStringLiteral("pos"));
            QCOMPARE(track.size(), 2);
        }
        QCOMPARE(m.evaluatedParam(QStringLiteral("pos"), 5).toDouble(), 50.0);
    }
};

QTEST_MAIN(TestEffectModel)
#include "test_effect_model.moc"
