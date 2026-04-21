
import pathlib

def replace_in_file(path, old, new):
    p = pathlib.Path(path)
    if not p.exists():
        print(f"File not found: {path}")
        return
    text = p.read_text(encoding="utf-8")
    if old in text:
        text = text.replace(old, new)
        p.write_text(text, encoding="utf-8")
        print(f"Patched: {path}")
    else:
        print(f"Old text not found in: {path}")

replace_in_file("engine/plugin/audio_plugin_host.hpp",
                'sm.value(QStringLiteral("defaultProjectSampleRate"), 48000).toDouble()',
                'sm.getDouble("defaultProjectSampleRate", 48000.0)')
replace_in_file("engine/plugin/audio_plugin_host.hpp",
                'sm.value(QStringLiteral("audioPluginMaxBlockSize"), 512).toInt()',
                'sm.getInt("audioPluginMaxBlockSize", 512)')

replace_in_file("engine/audio_mixer.cpp",
                'Rina::Core::SettingsManager::instance().value(QStringLiteral("_runtime_projectSampleRate"), 48000).toInt()',
                'Rina::Core::SettingsManager::instance().getInt("_runtime_projectSampleRate", 48000)')

replace_in_file("ui/include/project_service.hpp",
                'const auto &settings = Rina::Core::SettingsManager::instance().settings();',
                '')
replace_in_file("ui/include/project_service.hpp",
                'Rina::Core::SettingsManager::instance().setValue(QStringLiteral("_runtime_projectSampleRate"), m_sampleRate);',
                'Rina::Core::SettingsManager::instance().setValue("_runtime_projectSampleRate", m_sampleRate);')

replace_in_file("ui/src/clip_model.cpp",
                'EffectRegistry::instance().getEffect(clip->type)',
                'EffectRegistry::instance().getEffect(clip->type.toStdString())')
replace_in_file("ui/src/clip_model.cpp",
                'auto meta = Rina::Core::EffectRegistry::instance().getEffect("clip-" + type);',
                'auto meta = Rina::Core::EffectRegistry::instance().getEffect("clip-" + type.toStdString());')

replace_in_file("ui/src/timeline/timeline_clip.cpp",
                'auto meta = Rina::Core::EffectRegistry::instance().getEffect(type);',
                'auto meta = Rina::Core::EffectRegistry::instance().getEffect(type.toStdString());')

replace_in_file("core/src/effect_registry.cpp",
                'v.isbool()',
                'v.is_boolean()')

replace_in_file("core/src/image_decoder.cpp",
                'MediaDecoder(clipId, std::move(source), parent)',
                'MediaDecoder(clipId, source, parent)')

replace_in_file("core/src/video_decoder.cpp",
                'MediaDecoder(clipId, std::move(source), parent)',
                'MediaDecoder(clipId, source, parent)')
replace_in_file("core/src/video_decoder.cpp",
                'MediaDecoder(clipId, std::move(source)), m_store(store)',
                'MediaDecoder(clipId, source), m_store(store)')
replace_in_file("core/src/video_decoder.cpp",
                'mhwDevCtx', 'mhwDeviceCtx')
replace_in_file("core/src/video_decoder.cpp",
                'mconfigw', 'mconfig.width')
replace_in_file("core/src/video_decoder.cpp",
                'mconfigh', 'mconfig.height')
replace_in_file("core/src/video_decoder.cpp",
                'mcacheOrder.', 'mcacheOrderList.')

replace_in_file("core/src/video_encoder.cpp",
                'void VideoEncoder::cleanup()',
                'void VideoEncoder::close()')
replace_in_file("core/src/video_encoder.cpp",
                'task.frame', 'task.buf')
replace_in_file("core/src/video_encoder.cpp",
                'task.pts', 'task.pts')
replace_in_file("core/src/video_encoder.cpp",
                'void VideoEncoder::writeHeader()',
                'bool VideoEncoder::writeHeader()')
replace_in_file("core/src/video_encoder.cpp",
                'if (task.isFlush)',
                'if (!task.buf.data)') 
replace_in_file("core/src/video_encoder.cpp",
                'flushEncoder()', 'close()')
replace_in_file("core/src/video_encoder.cpp",
                'flushAudio()', 'close()')
