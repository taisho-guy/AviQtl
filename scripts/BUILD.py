#!/usr/bin/env python3
import os
import subprocess
import shutil
import sys

def run_command(args, cwd):
    print(f"Executing: {' '.join(args)} in {cwd}")
    subprocess.run(args, cwd=cwd, check=True)

def get_optimal_job_count():

    cpu_count = os.cpu_count() or 1
    
    try:
        mem_bytes = os.sysconf('SC_PAGE_SIZE') * os.sysconf('SC_PHYS_PAGES')
        total_mem_gb = mem_bytes / (1024**3)
    except (AttributeError, ValueError):
        total_mem_gb = 8.0 

    ram_per_job = 2.5
    mem_limited_jobs = max(1, int(total_mem_gb / ram_per_job))
    
    optimal_jobs = min(cpu_count, mem_limited_jobs)
    
    print(f"--- Resource Management ---")
    print(f"Detected: CPU {cpu_count} cores, RAM {total_mem_gb:.1f}GB")
    print(f"Parallel jobs limited to: {optimal_jobs} (to prevent OOM freeze)")
    print(f"---------------------------")
    return optimal_jobs

def build_project():
    scripts_dir = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.abspath(os.path.join(scripts_dir, '..'))
    app_dir = os.path.join(root_dir, 'app')
    build_dir = os.path.join(root_dir, 'build')

    print("=== Step 1: Updating dependencies (bgfx, bimg, bx) ===")
    run_command([sys.executable, os.path.join(scripts_dir, "update.py")], root_dir)

    print("=== Step 2: Configuring CMake with Clang ===")
    os.makedirs(build_dir, exist_ok=True)

    cmake_config = [
        "cmake",
        "-B", build_dir,
        "-S", root_dir,
        "-DCMAKE_CXX_COMPILER=clang++",
        "-DCMAKE_C_COMPILER=clang",
        "-DCMAKE_BUILD_TYPE=Debug"
    ]
    run_command(cmake_config, root_dir)

    print("=== Step 3: Compiling project ===")
    jobs = get_optimal_job_count()
    cmake_build = [
        "cmake",
        "--build", build_dir,
        "--parallel", str(jobs)
    ]
    run_command(cmake_build, root_dir)

    print(f"\nBuild complete! Output located in: {build_dir}")

if __name__ == "__main__":
    try:
        build_project()
    except subprocess.CalledProcessError as e:
        print(f"\n[ERROR] Build failed execution (Exit Code: {e.returncode})")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n[INFO] Build interrupted by user.")
        sys.exit(1)