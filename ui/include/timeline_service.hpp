#pragma once
#include <QObject>
#include <QUndoStack>
#include <memory>
#include "timeline_types.hpp"

namespace Rina::UI {
    class SelectionService;

    class TimelineService : public QObject {
        Q_OBJECT
    public:
        explicit TimelineService(SelectionService* selection, QObject* parent = nullptr);

        // Data Access
        const QList<ClipData>& clips() const { return m_clips; }
        QList<ClipData>& clipsMutable() { return m_clips; } // For serializer
        QUndoStack* undoStack() const { return m_undoStack; }

        // Operations (Public API)
        void undo();
        void redo();
        void createClip(const QString& type, int startFrame, int layer);
        void deleteClip(int clipId);
        void updateClip(int id, int layer, int startFrame, int duration);
        void splitClip(int clipId, int frame);
        
        // Effects
        void addEffect(int clipId, const QString& effectId);
        void removeEffect(int clipId, int effectIndex);
        void updateEffectParam(int clipId, int effectIndex, const QString& paramName, const QVariant& value);
        
        // Clipboard
        void copyClip(int clipId);
        void cutClip(int clipId);
        void pasteClip(int frame, int layer);
        
        // Internal (for Commands)
        void createClipInternal(const QString& type, int startFrame, int layer);
        void updateClipInternal(int id, int layer, int startFrame, int duration);
        void addEffectInternal(int clipId, const EffectData& effectData);
        void removeEffectInternal(int clipId, int effectIndex);
        void updateEffectParamInternal(int clipId, int effectIndex, const QString& paramName, const QVariant& value);
        
        // Helpers
        EffectData createEffectData(const QString& id);
        ClipData deepCopyClip(const ClipData& source);

        // State Management
        int nextClipId() const { return m_nextClipId; }
        void setNextClipId(int id) { m_nextClipId = id; }

    signals:
        void clipsChanged();
        void clipEffectsChanged(int clipId);
        void clipCreated(int id, int layer, int startFrame, int duration, const QString& type);

    private:
        QList<ClipData> m_clips;
        int m_nextClipId = 1;
        QUndoStack* m_undoStack;
        std::unique_ptr<ClipData> m_clipboard;
        SelectionService* m_selection;
    };
}