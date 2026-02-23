#pragma once
#include "settings_manager.hpp"
#include <QObject>

namespace Rina::UI {

class ProjectService : public QObject {
    Q_OBJECT
    Q_PROPERTY(int width READ width WRITE setWidth NOTIFY widthChanged)
    Q_PROPERTY(int height READ height WRITE setHeight NOTIFY heightChanged)
    Q_PROPERTY(double fps READ fps WRITE setFps NOTIFY fpsChanged)
    Q_PROPERTY(int sampleRate READ sampleRate WRITE setSampleRate NOTIFY sampleRateChanged)
  public:
    explicit ProjectService(QObject *parent = nullptr) : QObject(parent) {
        const auto &settings = Rina::Core::SettingsManager::instance().settings();
        m_width = settings.value("defaultProjectWidth", 1920).toInt();
        m_height = settings.value("defaultProjectHeight", 1080).toInt();
        m_fps = settings.value("defaultProjectFps", 60.0).toDouble();
        m_sampleRate = settings.value("defaultProjectSampleRate", 48000).toInt();
        Rina::Core::SettingsManager::instance().setValue("_runtime_projectSampleRate", m_sampleRate);
    }

    int width() const { return m_width; }
    void setWidth(int w) {
        if (m_width == w)
            return;
        m_width = w;
        emit widthChanged();
    }

    int height() const { return m_height; }
    void setHeight(int h) {
        if (m_height == h)
            return;
        m_height = h;
        emit heightChanged();
    }

    double fps() const { return m_fps; }
    void setFps(double f) {
        if (qFuzzyCompare(m_fps, f))
            return;
        m_fps = f;
        emit fpsChanged();
    }

    int sampleRate() const { return m_sampleRate; }
    void setSampleRate(int r) {
        if (m_sampleRate == r)
            return;
        m_sampleRate = r;
        Rina::Core::SettingsManager::instance().setValue("_runtime_projectSampleRate", m_sampleRate);
        emit sampleRateChanged();
    }

  signals:
    void widthChanged();
    void heightChanged();
    void fpsChanged();
    void sampleRateChanged();

  private:
    int m_width;
    int m_height;
    double m_fps;
    int m_sampleRate;
};
} // namespace Rina::UI