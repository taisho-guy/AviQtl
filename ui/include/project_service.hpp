#pragma once
#include <QObject>

namespace Rina::UI {

    class ProjectService : public QObject {
        Q_OBJECT
        Q_PROPERTY(int width READ width WRITE setWidth NOTIFY widthChanged)
        Q_PROPERTY(int height READ height WRITE setHeight NOTIFY heightChanged)
        Q_PROPERTY(double fps READ fps WRITE setFps NOTIFY fpsChanged)
        Q_PROPERTY(int totalFrames READ totalFrames WRITE setTotalFrames NOTIFY totalFramesChanged)
    public:
        explicit ProjectService(QObject* parent = nullptr) 
            : QObject(parent) {}

        int width() const { return m_width; }
        void setWidth(int w) {
            if (m_width == w) return;
            m_width = w;
            emit widthChanged();
        }

        int height() const { return m_height; }
        void setHeight(int h) {
            if (m_height == h) return;
            m_height = h;
            emit heightChanged();
        }

        double fps() const { return m_fps; }
        void setFps(double f) {
            if (qFuzzyCompare(m_fps, f)) return;
            m_fps = f;
            emit fpsChanged();
        }

        int totalFrames() const { return m_totalFrames; }
        void setTotalFrames(int f) {
            if (m_totalFrames == f) return;
            m_totalFrames = f;
            emit totalFramesChanged();
        }

    signals:
        void widthChanged();
        void heightChanged();
        void fpsChanged();
        void totalFramesChanged();

    private:
        int m_width = 1920;
        int m_height = 1080;
        double m_fps = 60.0;
        int m_totalFrames = 3600;
    };
}