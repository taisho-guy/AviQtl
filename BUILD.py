import sys
import os
import subprocess
import shutil
import multiprocessing
from pathlib import Path
from PySide6 import QtWidgets, QtCore, QtGui
import platform

class BuildWorker(QtCore.QThread):
    progress_signal = QtCore.Signal(int, str)
    log_signal = QtCore.Signal(str)
    finished_signal = QtCore.Signal(bool, str)
    dist_dir = None

    def __init__(self, source_dir, temp_base, output_dir, is_debug=False):
        super().__init__()
        self.source_dir = source_dir
        self.temp_base = temp_base
        self.output_dir = output_dir
        self.is_debug = is_debug
        self.dist_dir = self.source_dir / "dist"
        self.system = platform.system()

    def _run_cmd(self, cmd, shell=False):
        self.log_signal.emit(f"Run: {' '.join(cmd) if isinstance(cmd, list) else cmd}")
        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding='utf-8',
            errors='replace',
            shell=shell
        )
        for line in process.stdout:
            self.log_signal.emit(line.rstrip())
        process.wait()
        if process.returncode != 0:
            raise subprocess.CalledProcessError(process.returncode, cmd)

    def install_dependencies(self):
        self.log_signal.emit("依存関係を確認中...")
        
        if self.system == "Windows":
            # MSYS2 (UCRT64)
            if "MSYSTEM" in os.environ:
                self.log_signal.emit("MSYS2 環境を検出しました。")
                deps = [
                    "mingw-w64-ucrt-x86_64-toolchain",
                    "mingw-w64-ucrt-x86_64-cmake",
                    "mingw-w64-ucrt-x86_64-ninja",
                    "git",
                    "mingw-w64-ucrt-x86_64-qt6-base",
                    "mingw-w64-ucrt-x86_64-qt6-declarative",
                    "mingw-w64-ucrt-x86_64-qt6-quick3d",
                    "mingw-w64-ucrt-x86_64-qt6-multimedia",
                    "mingw-w64-ucrt-x86_64-qt6-shadertools",
                    "mingw-w64-ucrt-x86_64-qt6-svg",
                    "mingw-w64-ucrt-x86_64-qt6-5compat",
                    "mingw-w64-ucrt-x86_64-qt6-tools",
                    "mingw-w64-ucrt-x86_64-ffmpeg",
                    "mingw-w64-ucrt-x86_64-luajit",
                    "mingw-w64-ucrt-x86_64-vulkan-loader",
                    "mingw-w64-ucrt-x86_64-vulkan-headers",
                    "mingw-w64-ucrt-x86_64-pkg-config",
                    "mingw-w64-ucrt-x86_64-mold",
                    "mingw-w64-ucrt-x86_64-lilv",
                    "mingw-w64-ucrt-x86_64-ladspa-sdk",
                    "mingw-w64-ucrt-x86_64-curl",
                    "zip"
                ]
                self._run_cmd(["pacman", "-S", "--needed", "--noconfirm"] + deps)
                
                # Audio Plugin SDKs
                if not (self.source_dir / "clap").exists():
                    self._run_cmd(["git", "clone", "https://github.com/free-audio/clap.git", "--depth", "1"])

            else:
                self.log_signal.emit("警告: MSYS2 環境外で実行されています。依存関係の自動インストールはスキップされます。")

        elif self.system == "Linux":
            # CachyOS (Arch Linux based)
            if shutil.which("pacman"):
                self.log_signal.emit("Pacman (Arch/CachyOS) を検出しました。")
                deps = [
                    "base-devel", "git", "cmake", "ninja", "extra/clang", "mold", "zip",
                    "mesa", "vulkan-devel", "libxkbcommon", "wayland", "wayland-protocols",
                    "libffi", "ffmpeg", "luajit", "fftw", "qt6-base", "qt6-declarative",
                    "qt6-quick3d", "qt6-multimedia", "qt6-shadertools", "qt6-svg",
                    "qt6-5compat", "qt6-tools", "lilv", "ladspa", "clap", "vst3sdk"
                ]
                # sudoが必要な場合があるため、パスワード入力なしで実行できるか、あるいはユーザーに委ねる
                # ここではsudoを使ってインストールを試みる
                try:
                    self._run_cmd(["sudo", "pacman", "-S", "--needed", "--noconfirm"] + deps)
                except Exception as e:
                    self.log_signal.emit(f"依存関係のインストールに失敗しました (sudo権限が必要かもしれません): {e}")
            else:
                self.log_signal.emit("警告: Pacmanが見つかりません。依存関係の自動インストールはスキップされます。")

    def run(self):
        try:
            cpu_count = multiprocessing.cpu_count()
            j_slots = cpu_count

            self.log_signal.emit(f"リソース割り当て: {j_slots} 並列ジョブ")
            
            # 依存関係のインストール
            self.install_dependencies()
            
            name = "Debug" if self.is_debug else "Release"
            self.progress_signal.emit(10, f"{name} ビルド開始")
            self.log_signal.emit(f"ソースディレクトリ: {self.source_dir}")
            
            work_dir = self.temp_base / name
            
            # 1. CMake Configuration
            self.log_signal.emit(f"--- {name} 設定 ({self.system}) ---")
            
            conf_cmd = [
                "cmake", "-B", str(work_dir), "-G", "Ninja",
                f"-DCMAKE_BUILD_TYPE={name}",
            ]

            if self.system == "Linux":
                conf_cmd.extend([
                    "-DCMAKE_C_COMPILER=clang",
                    "-DCMAKE_CXX_COMPILER=clang++",
                ])
                if not self.is_debug:
                    conf_cmd.append("-DCMAKE_CXX_FLAGS=-O3 -march=x86-64-v3 -flto -fno-semantic-interposition")
                    conf_cmd.append("-DCMAKE_POLICY_DEFAULT_CMP0056=NEW")
                    conf_cmd.append("-DCMAKE_SKIP_INSTALL_RPATH=ON")
            
            elif self.system == "Windows":
                 conf_cmd.append("-DCMAKE_BUILD_TYPE=Release")
                 # VST3/CLAP SDKのパス指定 (Windowsの場合、ソースディレクトリ直下にクローンしたと仮定)
                 conf_cmd.append(f"-DCLAP_SDK_DIR={self.source_dir}/clap")

            conf_cmd.append(str(self.source_dir))
            self._run_cmd(conf_cmd)

            # 3. Build
            self.log_signal.emit(f"{name} コンパイル中...")
            build_cmd = ["cmake", "--build", str(work_dir), "-j", str(j_slots)]
            self._run_cmd(build_cmd)

            # 4. パッケージング処理
            self.log_signal.emit(f"パッケージングを開始します...")
            self.output_dir.mkdir(parents=True, exist_ok=True)
            self.dist_dir.mkdir(parents=True, exist_ok=True)
            
            exe_name = "Rina.exe" if self.system == "Windows" else "Rina"
            dest_bin = self.output_dir / exe_name
            
            # Windowsの場合、ビルド成果物は直下にあるとは限らない (Release/Debugサブディレクトリなど)
            # Ninjaジェネレーターなら直下にあるはず
            src_bin = work_dir / exe_name
            
            if dest_bin.exists():
                dest_bin.unlink()
            
            if src_bin.exists():
                shutil.copy2(src_bin, dest_bin)
            else:
                self.log_signal.emit(f"エラー: 実行ファイルが見つかりません: {src_bin}")
                # 続行不能
                raise FileNotFoundError(f"{src_bin} not found")

            # アセットのコピー (アプリ動作に必要なデータのみ)
            for d in ["effects", "objects"]:
                src_path = self.source_dir / "ui/qml" / d
                dest_path = self.output_dir / d
                if src_path.exists():
                    if dest_path.exists(): shutil.rmtree(dest_path)
                    # Linuxの場合は不要なシェーダーソースを除外
                    ignore_pat = shutil.ignore_patterns("*.frag", "*.vert", "*.glsl") if self.system == "Linux" else None
                    shutil.copytree(src_path, dest_path, ignore=ignore_pat)

            # コンパイル済みシェーダー(.qsb)のコピー
            for d in ["effects", "objects"]:
                qsb_src_dir = work_dir / "ui" / "qml" / d # ビルドディレクトリ構造に合わせる
                qsb_dest_dir = self.output_dir / d
                if qsb_src_dir.exists() and qsb_dest_dir.exists():
                    for qsb_file in qsb_src_dir.glob("*.qsb"):
                        shutil.copy2(qsb_file, qsb_dest_dir / qsb_file.name)

            # Plugins
            plugins_src = self.source_dir / "plugins"
            if plugins_src.exists() and any(plugins_src.iterdir()):
                plugins_dest = self.output_dir / "plugins"
                if plugins_src.exists():
                    if plugins_dest.exists(): shutil.rmtree(plugins_dest)
                    shutil.copytree(plugins_src, plugins_dest)

            # Windows固有のデプロイ処理 (windeployqtなど)
            if self.system == "Windows":
                self.log_signal.emit("Windows デプロイメント処理を実行中...")
                # windeployqtの実行
                # 注意: PATHにwindeployqtが含まれている必要がある
                qml_dir = self.source_dir / "ui/qml"
                deploy_cmd = [
                    "windeployqt",
                    "--qmldir", str(qml_dir),
                    "--no-translations",
                    "--no-compiler-runtime",
                    "--release" if not self.is_debug else "--debug",
                    str(dest_bin),
                    "--dir", str(self.output_dir)
                ]
                self._run_cmd(deploy_cmd)
                
                # qt.confの作成
                qt_conf = self.output_dir / "qt.conf"
                with open(qt_conf, "w") as f:
                    f.write("[Paths]\nPlugins = .\n")
                
                # 非Qt依存関係の収集 (簡易的)
                # lddの代わりに、MSYS2環境なら依存DLLをコピーするロジックが必要だが、
                # ここではwindeployqtが主要なQt依存関係を処理したと仮定する。
                # 必要であれば、objdump等で依存関係を解析してコピーする処理を追加する。

            # ZIP圧縮
            self.log_signal.emit("アーカイブを作成中...")
            archive_name = f"Rina-{self.system}-x64"
            if self.system == "Linux":
                archive_name = "Rina-Linux-x86_64-v3"
            
            shutil.make_archive(
                str(self.dist_dir / archive_name),
                'zip',
                root_dir=self.output_dir
            )

            self.log_signal.emit(f"ビルド完了：{dest_bin}")
            self.log_signal.emit(f"アーカイブ：{self.dist_dir / (archive_name + '.zip')}")
            if self.system == "Windows":
                 self.log_signal.emit("ヒント：buildフォルダ内のRina.exeを実行してください")
            else:
                 self.log_signal.emit("ヒント：python BUILD.py --runで直ちに起動できます")
            
            self.progress_signal.emit(100, "完了")
            self.finished_signal.emit(True, f"ビルド成功")
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
        
        self.status_label = QtWidgets.QLabel("ステータス：準備完了")
        layout.addWidget(self.status_label)
        
        self.progress_bar = QtWidgets.QProgressBar()
        layout.addWidget(self.progress_bar)
        
        self.log_view = QtWidgets.QPlainTextEdit()
        self.log_view.setReadOnly(True)
        layout.addWidget(self.log_view)
        
        btn_layout = QtWidgets.QHBoxLayout()
        self.btn_build = QtWidgets.QPushButton("リリースビルド")
        self.btn_build.clicked.connect(self.run_build)
        btn_layout.addWidget(self.btn_build)
        layout.addLayout(btn_layout)
        
        self.setCentralWidget(central)

    def run_build(self):
        self.set_ui_enabled(False)
        self.worker = BuildWorker(self.source_dir, self.temp_base, self.output_dir)
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
            self.log_view.appendPlainText(f"\nエラー: {msg}")

    def set_ui_enabled(self, enabled):
        self.btn_build.setEnabled(enabled)

if __name__ == "__main__":
    if "--gui" in sys.argv:
        app = QtWidgets.QApplication(sys.argv)
        gui = RinaGui()
        gui.show()
        sys.exit(app.exec())
    else:
        # CLI Mode (Release Build)
        app = QtCore.QCoreApplication(sys.argv)
        
        source_dir = Path.cwd()
        temp_base = source_dir / ".build_tmp"
        output_dir = source_dir / "build"
        
        # Host Native Release Build
        worker = BuildWorker(source_dir, temp_base, output_dir, is_debug=False)
        
        worker.log_signal.connect(print)
        worker.progress_signal.connect(lambda val, msg: print(f"[{val}%] {msg}"))
        worker.finished_signal.connect(lambda success, msg: app.quit() if success else app.exit(1))
        
        print("リリースビルドを開始します...")
        worker.start()
        sys.exit(app.exec())