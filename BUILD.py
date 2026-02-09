import sys
import os
import subprocess
import shutil
import multiprocessing
from pathlib import Path
from PySide6 import QtWidgets, QtCore, QtGui

try:
    import psutil
    HAS_PSUTIL = True
except ImportError:
    HAS_PSUTIL = False

class BuildWorker(QtCore.QThread):
    progress_signal = QtCore.Signal(int, str)
    log_signal = QtCore.Signal(str)
    finished_signal = QtCore.Signal(bool, str)

    def __init__(self, targets, source_dir, temp_base, output_dir, is_debug=False):
        super().__init__()
        self.targets = targets
        self.source_dir = source_dir
        self.temp_base = temp_base
        self.output_dir = output_dir
        self.is_debug = is_debug

    def _run_cmd(self, cmd):
        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding='utf-8',
            errors='replace'
        )
        for line in process.stdout:
            self.log_signal.emit(line.rstrip())
        process.wait()
        if process.returncode != 0:
            raise subprocess.CalledProcessError(process.returncode, cmd)

    def run(self):
        try:
            # メモリ保護ロジック
            cpu_count = multiprocessing.cpu_count()
            if HAS_PSUTIL:
                mem_gb = psutil.virtual_memory().total / (1024**3)
                j_slots = max(1, min(cpu_count // 2, int(mem_gb // 4)))
            else:
                j_slots = max(1, cpu_count // 2) # 安全策

            self.log_signal.emit(f"🚀 リソース割り当て: {j_slots} 並列ジョブ")
            
            total_targets = len(self.targets)
            
            for i, (name, flags) in enumerate(self.targets):
                self.progress_signal.emit(int((i / total_targets) * 100), f"{name} をビルド中")
                
                work_dir = self.temp_base / name
                dest_dir = self.output_dir / name if not self.is_debug else self.output_dir / "debug"
                
                # 1. CMake Configuration
                self.log_signal.emit(f"--- {name} の設定中 ---")
                conf_cmd = [
                    "cmake", "-B", str(work_dir), "-G", "Ninja",
                    f"-DCMAKE_BUILD_TYPE={'Debug' if self.is_debug else 'Release'}",
                    f"-DARCH_FLAGS={flags}",
                    str(self.source_dir)
                ]
                self._run_cmd(conf_cmd)

                # 2. Build (nice を使用してシステム全体のレスポンスを維持)
                self.log_signal.emit(f"🔨 {name} をビルド中 (ジョブ数: {j_slots})")
                build_cmd = ["nice", "-n", "15", "cmake", "--build", str(work_dir), "-j", str(j_slots)]
                self._run_cmd(build_cmd)

                # 3. Deployment
                if dest_dir.exists(): shutil.rmtree(dest_dir)
                dest_dir.mkdir(parents=True, exist_ok=True)
                
                shutil.copy2(work_dir / "Rina", dest_dir / "Rina")
                for d in ["effects", "objects"]:
                    src_path = self.source_dir / "ui/qml" / d
                    if src_path.exists():
                        shutil.copytree(src_path, dest_dir / d)

                # Copy plugins
                plugins_src = self.source_dir / "plugins"
                if plugins_src.exists():
                    shutil.copytree(plugins_src, dest_dir / "plugins")

                # 4. Compression (Releaseのみ)
                if not self.is_debug:
                    self.log_signal.emit(f"📦 {name} をアーカイブ中 (7z LZMA2 Max)")
                    archive_name = self.output_dir / f"Rina_Linux_{name}.7z"
                    self._run_cmd([
                        "7z", "a", "-t7z", "-m0=lzma2", "-mx=9",
                        str(archive_name), f"{dest_dir}/*"
                    ])
                    shutil.rmtree(dest_dir) # 圧縮後はディレクトリを削除

            self.progress_signal.emit(100, "完了")
            self.finished_signal.emit(True, "すべてのビルドが完了しました。")
        except Exception as e:
            self.finished_signal.emit(False, str(e))

class RinaGui(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()
        self.source_dir = Path.cwd()
        self.temp_base = self.source_dir / ".build_tmp"
        self.output_dir = self.source_dir / "build"
        
        # ターゲットリスト: (名称, marchフラグ)
        self.targets = [
            # --- Generic x86-64 Levels ---
            ("x86-64-v2", "-march=x86-64-v2"), # Nehalem以降
            ("x86-64-v3", "-march=x86-64-v3"), # Haswell/AVX2以降
            ("x86-64-v4", "-march=x86-64-v4"), # AVX-512

            # --- AMD ZEN Family ---
            ("znver1", "-march=znver1"),
            ("znver2", "-march=znver2"),
            ("znver3", "-march=znver3"),
            ("znver4", "-march=znver4"), # Your 8840U native target

            # --- Intel Family ---
            ("skylake", "-march=skylake"),
            ("icelake-client", "-march=icelake-client"),
            ("alderlake", "-march=alderlake"),
            ("sapphirerapids", "-march=sapphirerapids"),

            # --- Others ---
            ("aarch64", "--target=aarch64-linux-gnu -march=armv8-a"),
            ("riscv64", "--target=riscv64-linux-gnu -march=rv64gc"),
        ]
        
        self.init_ui()

    def init_ui(self):
        self.setWindowTitle("Rina ビルドシステム")
        self.resize(600, 400)
        
        central = QtWidgets.QWidget()
        layout = QtWidgets.QVBoxLayout(central)
        
        self.status_label = QtWidgets.QLabel("ステータス: 準備完了")
        layout.addWidget(self.status_label)
        
        self.progress_bar = QtWidgets.QProgressBar()
        layout.addWidget(self.progress_bar)
        
        self.log_view = QtWidgets.QPlainTextEdit()
        self.log_view.setReadOnly(True)
        layout.addWidget(self.log_view)
        
        btn_layout = QtWidgets.QHBoxLayout()
        self.btn_full = QtWidgets.QPushButton("🚀 全てビルド & 圧縮")
        self.btn_full.clicked.connect(self.run_full_build)
        
        self.btn_debug = QtWidgets.QPushButton("🛠 ホストデバッグ")
        self.btn_debug.clicked.connect(self.run_debug_build)
        
        btn_layout.addWidget(self.btn_full)
        btn_layout.addWidget(self.btn_debug)
        layout.addLayout(btn_layout)
        
        self.setCentralWidget(central)

    def run_full_build(self):
        self.set_ui_enabled(False)
        self.worker = BuildWorker(self.targets, self.source_dir, self.temp_base, self.output_dir)
        self.connect_worker()
        self.worker.start()

    def run_debug_build(self):
        self.set_ui_enabled(False)
        self.worker = BuildWorker([("host", "-march=native")], self.source_dir, self.temp_base, self.output_dir, True)
        self.connect_worker()
        self.worker.start()

    def connect_worker(self):
        self.worker.progress_signal.connect(self.update_progress)
        self.worker.log_signal.connect(self.log_view.appendPlainText)
        self.worker.finished_signal.connect(self.on_finished)

    def update_progress(self, val, msg):
        self.progress_bar.setValue(val)
        self.status_label.setText(f"Status: {msg}")

    def on_finished(self, success, msg):
        self.set_ui_enabled(True)
        if success:
            QtWidgets.QMessageBox.information(self, "成功", "ビルドが完了しました！")
        else:
            self.log_view.appendPlainText(f"\n❌ エラー: {msg}")

    def set_ui_enabled(self, enabled):
        self.btn_full.setEnabled(enabled)
        self.btn_debug.setEnabled(enabled)

if __name__ == "__main__":
    if "--gui" in sys.argv:
        app = QtWidgets.QApplication(sys.argv)
        gui = RinaGui()
        gui.show()
        sys.exit(app.exec())
    else:
        # CLI Mode (Debug Build)
        app = QtCore.QCoreApplication(sys.argv)
        
        source_dir = Path.cwd()
        temp_base = source_dir / ".build_tmp"
        output_dir = source_dir / "build"
        
        # Host Native Debug Build
        targets = [("host", "-march=native")]
        worker = BuildWorker(targets, source_dir, temp_base, output_dir, is_debug=True)
        
        worker.log_signal.connect(print)
        worker.progress_signal.connect(lambda val, msg: print(f"[{val}%] {msg}"))
        worker.finished_signal.connect(lambda success, msg: app.quit() if success else app.exit(1))
        
        print("🚀 CLIデバッグビルドを開始します...")
        worker.start()
        sys.exit(app.exec())