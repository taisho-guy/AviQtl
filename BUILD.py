import sys
import os
import subprocess
import shutil
import multiprocessing
from pathlib import Path
from PySide6 import QtCore
import platform
import shlex
import concurrent.futures

class BuildWorker(QtCore.QThread):
    progress_signal = QtCore.Signal(int, str)
    log_signal = QtCore.Signal(str)
    finished_signal = QtCore.Signal(bool, str)
    dist_dir = None

    def __init__(self, source_dir, temp_base, output_dir, is_debug=False, use_container=True, do_tidy=False):
        super().__init__()
        self.source_dir = source_dir
        self.temp_base = temp_base
        self.output_dir = output_dir
        self.is_debug = is_debug
        self.do_tidy = do_tidy
        self.use_container_opt = use_container
        self.dist_dir = self.source_dir / "dist"
        self.system = platform.system()
        self.container_name = "archlinux-rina"

    def _run_cmd(self, cmd, shell=False, in_container=False):
        display_cmd = ' '.join(cmd) if isinstance(cmd, list) else cmd
        prefix = "[Container] " if in_container else ""
        self.log_signal.emit(f"{prefix}Run: {display_cmd}")

        actual_cmd = cmd
        if in_container and self.system == "Linux":
            if isinstance(cmd, str):
                cmd = shlex.split(cmd)
            
            # コンテナ内でカレントディレクトリを維持して実行
            cmd_str = shlex.join(cmd)
            cwd = os.getcwd()
            # プロセスをシェルで包んで実行 (shell=True)
            inner_cmd = f"cd {shlex.quote(cwd)} && {cmd_str}"
            actual_cmd = f"distrobox enter {self.container_name} -- bash -c {shlex.quote(inner_cmd)}"
            shell = True

        process = subprocess.Popen(
            actual_cmd,
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
            raise subprocess.CalledProcessError(process.returncode, actual_cmd)

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
                    "zip",
                    "mingw-w64-ucrt-x86_64-clang-tools-extra"
                ]
                self._run_cmd(["pacman", "-Syu", "--needed", "--noconfirm"] + deps)
                
                # Audio Plugin SDKs
                if not (self.source_dir / "clap").exists():
                    self._run_cmd(["git", "clone", "https://github.com/free-audio/clap.git", "--depth", "1"])

            else:
                self.log_signal.emit("警告: MSYS2 環境外で実行されています。依存関係の自動インストールはスキップされます。")

        elif self.system == "Linux":
            if not self.use_container_opt:
                self.log_signal.emit("コンテナビルドは無効です。ホスト環境でビルドします。")
                return

            # 1. ホスト環境: distrobox と podman の確認
            if not (shutil.which("distrobox") and shutil.which("podman")):
                self.log_signal.emit("警告: distrobox または podman が見つかりません。インストールしてください。")

            # 2. コンテナの作成 (archlinux-rina)
            self.log_signal.emit(f"ビルド用コンテナ '{self.container_name}' を準備中...")
            try:
                self._run_cmd(["distrobox", "create", "--name", self.container_name, "--image", "archlinux:latest", "--yes"])
            except subprocess.CalledProcessError:
                self.log_signal.emit("コンテナの作成をスキップします（既に存在するかエラーが発生しました）。")

            # 3. コンテナ内: ビルド依存関係のインストール
            self.log_signal.emit("コンテナ内の依存関係をインストール中...")
            
            container_deps = [
                "base-devel", "git", "cmake", "ninja", "clang", "mold", "zip",
                "mesa", "vulkan-devel", "libxkbcommon", "wayland", "wayland-protocols",
                "libffi", "ffmpeg", "luajit", "fftw", "qt6-base", "qt6-declarative",
                "qt6-quick3d", "qt6-multimedia", "qt6-shadertools", "qt6-svg",
                "qt6-5compat", "qt6-tools", "lilv", "ladspa", "clap", "vst3sdk", "carla", "kirigami", "openmp", "extra-cmake-modules", "kcolorscheme"
            ]
            # コンテナ内では sudo を使用してインストール
            # ロックファイルの解除を試みる (過去のプロセス中断時の対策)
            try:
                self._run_cmd(["sudo", "rm", "-f", "/var/lib/pacman/db.lck"], in_container=True)
            except Exception:
                pass

            self._run_cmd(["sudo", "pacman", "-Syu", "--needed", "--noconfirm"] + container_deps, in_container=True)
    def _run_tidy_single(self, file, build_dir):
        """個別のファイルに対してclang-tidyを実行する（並列実行用）"""
        cmd = ["clang-tidy", "-p", str(build_dir), "--fix", "--format-style=file", "--quiet", file]
        
        use_container = (self.system == "Linux" and self.use_container_opt)
        actual_cmd = cmd
        shell = False
        
        if use_container:
            cmd_str = shlex.join(cmd)
            cwd = os.getcwd()
            inner_cmd = f"cd {shlex.quote(cwd)} && {cmd_str}"
            actual_cmd = f"distrobox enter {self.container_name} -- bash -c {shlex.quote(inner_cmd)}"
            shell = True
            
        # ログ混雑を防ぐため標準出力を捨てる
        subprocess.run(actual_cmd, shell=shell, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)

    def run_clang_tidy(self, build_dir):
        self.log_signal.emit("clang-tidy による静的解析と自動修正を開始します...")
        
        cc_json = build_dir / "compile_commands.json"
        if not cc_json.exists():
            self.log_signal.emit("警告: compile_commands.json が見つかりません。clang-tidyをスキップします。")
            return

        import json
        with open(cc_json, 'r', encoding='utf-8') as f:
            commands = json.load(f)

        files_to_check = []
        for entry in commands:
            file_path = Path(entry['file'])
            # ソースディレクトリ以下のファイルのみ対象（外部ライブラリや自動生成ファイルを除外）
            if self.source_dir in file_path.parents:
                if "qrc_" in file_path.name or "moc_" in file_path.name:
                    continue
                files_to_check.append(str(file_path))

        if not files_to_check:
            self.log_signal.emit("チェック対象のファイルがありません。")
            return

        self.log_signal.emit(f"{len(files_to_check)} ファイルを並列処理中...")
        
        max_workers = multiprocessing.cpu_count()
        with concurrent.futures.ThreadPoolExecutor(max_workers=max_workers) as executor:
            # 全ファイルを並列で処理
            list(executor.map(lambda f: self._run_tidy_single(f, build_dir), files_to_check))

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
                    conf_cmd.append("-DCMAKE_CXX_FLAGS=-O3 -flto -fno-semantic-interposition")
                    conf_cmd.append("-DCMAKE_POLICY_DEFAULT_CMP0056=NEW")
                    conf_cmd.append("-DCMAKE_SKIP_INSTALL_RPATH=ON")
            
            elif self.system == "Windows":
                 conf_cmd.append("-DCMAKE_BUILD_TYPE=Release")
                 # VST3/CLAP SDKのパス指定 (Windowsの場合、ソースディレクトリ直下にクローンしたと仮定)
                 conf_cmd.append(f"-DCLAP_SDK_DIR={self.source_dir}/clap")

            conf_cmd.append(str(self.source_dir))
            
            # Linuxの場合はコンテナ内で実行
            use_container = (self.system == "Linux" and self.use_container_opt)
            self._run_cmd(conf_cmd, in_container=use_container)

            # 2. Clang-Tidy (Optional)
            if self.do_tidy:
                self.run_clang_tidy(work_dir)

            # 3. Build
            self.log_signal.emit(f"{name} コンパイル中...")
            build_cmd = ["cmake", "--build", str(work_dir), "-j", str(j_slots)]
            self._run_cmd(build_cmd, in_container=use_container)

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
                qsb_src_dir = work_dir / d
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

if __name__ == "__main__":
    # CLI Mode (Release Build)
    app = QtCore.QCoreApplication(sys.argv)
    
    source_dir = Path.cwd()
    temp_base = source_dir / ".build_tmp"
    output_dir = source_dir / "build"
    
    # Host Native Release Build
    use_container = "--no-container" not in sys.argv
    do_tidy = "--fix-lint" in sys.argv
    
    worker = BuildWorker(source_dir, temp_base, output_dir, is_debug=False, use_container=use_container, do_tidy=do_tidy)
    
    worker.log_signal.connect(print)
    worker.progress_signal.connect(lambda val, msg: print(f"[{val}%] {msg}"))
    
    def on_cli_finished(success, msg):
        if success:
            app.quit()
        else:
            print(f"\nビルド失敗: {msg}")
            app.exit(1)
    worker.finished_signal.connect(on_cli_finished)
    
    print("リリースビルドを開始します...")
    worker.start()
    sys.exit(app.exec())