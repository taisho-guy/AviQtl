import sys
import os
import subprocess
import shutil
import multiprocessing
from pathlib import Path
from PySide6 import QtWidgets, QtCore, QtGui

# psutilは必須とする（開発環境なので）
import psutil

class BuildWorker(QtCore.QThread):
    progress_signal = QtCore.Signal(int, str)
    log_signal = QtCore.Signal(str)
    finished_signal = QtCore.Signal(bool, str)

    def __init__(self, source_dir, temp_base, output_dir, is_debug=False):
        super().__init__()
        self.source_dir = source_dir
        self.temp_base = temp_base
        self.output_dir = output_dir
        self.is_debug = is_debug

    def _run_cmd(self, cmd):
        # 開発者向けにコマンドをログ出力
        self.log_signal.emit(f"Run: {' '.join(cmd)}")
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
            # 1. リソース割り当て: Ryzen 8840U (16 threads) をフル活用
            # 開発機なので容赦なく全スレッドを使う
            cpu_count = multiprocessing.cpu_count()
            j_slots = cpu_count

            self.log_signal.emit(f"リソース割り当て: {j_slots} 並列ジョブ (Host Native)")
            
            name = "Debug" if self.is_debug else "Release"
            self.progress_signal.emit(10, f"{name} ビルド開始")
            
            # ビルドディレクトリを保持 (インクリメンタルビルドのため削除しない)
            work_dir = self.temp_base / name
            
            # 1. CMake Configuration
            self.log_signal.emit(f"--- {name} 設定---")
            
            conf_cmd = [
                "cmake", "-B", str(work_dir), "-G", "Ninja",
                f"-DCMAKE_BUILD_TYPE={name}",
                # コンパイラ強制 (システムデフォルトのClang)
                "-DCMAKE_C_COMPILER=clang",
                "-DCMAKE_CXX_COMPILER=clang++",
            ]

            # 究極のローカル最適化フラグ
            if not self.is_debug:
                conf_cmd.append("-DCMAKE_CXX_FLAGS=-O3 -march=native -mtune=native -flto=thin -fno-semantic-interposition")
                conf_cmd.append("-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld -Wl,--as-needed")

            conf_cmd.append(str(self.source_dir))
            self._run_cmd(conf_cmd)

            # 3. Build
            self.log_signal.emit(f"🔨 {name} コンパイル中...")
            # nice値は削除し、最高優先度で実行
            build_cmd = ["cmake", "--build", str(work_dir), "-j", str(j_slots)]
            self._run_cmd(build_cmd)

            # 4. 配布処理 (.build_tmp -> build)
            self.log_signal.emit(f"📂 成果物を {self.output_dir} にコピー中...")
            self.output_dir.mkdir(parents=True, exist_ok=True)

            dest_bin = self.output_dir / "Rina"
            if dest_bin.exists():
                dest_bin.unlink()
            shutil.copy2(work_dir / "Rina", dest_bin)

            # アセットのコピー
            for d in ["effects", "objects"]:
                src_path = self.source_dir / "ui/qml" / d
                dest_path = self.output_dir / d
                if src_path.exists():
                    if dest_path.exists(): shutil.rmtree(dest_path)
                    shutil.copytree(src_path, dest_path)

            # Plugins
            plugins_src = self.source_dir / "plugins"
            plugins_dest = self.output_dir / "plugins"
            if plugins_src.exists():
                if plugins_dest.exists(): shutil.rmtree(plugins_dest)
                shutil.copytree(plugins_src, plugins_dest)

            self.log_signal.emit(f"✅ ビルド完了: {dest_bin}")
            self.log_signal.emit("ヒント: ./BUILD.py --run で直ちに起動できます")
            
            self.progress_signal.emit(100, "完了")
            self.finished_signal.emit(True, f"ビルド成功: {dest_bin}")
        except Exception as e:
            self.finished_signal.emit(False, str(e))

class RinaGui(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()
        self.source_dir = Path.cwd()
        self.temp_base = self.source_dir / ".build_tmp"
        self.output_dir = self.source_dir / "build"
        
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
        self.btn_full = QtWidgets.QPushButton("最適化ビルド")
        self.btn_full.clicked.connect(self.run_full_build)
        
        self.btn_debug = QtWidgets.QPushButton("デバッグビルド")
        self.btn_debug.clicked.connect(self.run_debug_build)
        
        btn_layout.addWidget(self.btn_full)
        btn_layout.addWidget(self.btn_debug)
        layout.addLayout(btn_layout)
        
        self.setCentralWidget(central)

    def run_full_build(self):
        self.set_ui_enabled(False)
        self.worker = BuildWorker(self.source_dir, self.temp_base, self.output_dir)
        self.connect_worker()
        self.worker.start()

    def run_debug_build(self):
        self.set_ui_enabled(False)
        self.worker = BuildWorker(self.source_dir, self.temp_base, self.output_dir, True)
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
        worker = BuildWorker(source_dir, temp_base, output_dir, is_debug=True)
        
        worker.log_signal.connect(print)
        worker.progress_signal.connect(lambda val, msg: print(f"[{val}%] {msg}"))
        worker.finished_signal.connect(lambda success, msg: app.quit() if success else app.exit(1))
        
        print("CLIデバッグビルドを開始します...")
        worker.start()
        sys.exit(app.exec())