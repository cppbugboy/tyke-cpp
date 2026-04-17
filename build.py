#!/usr/bin/env python3
"""
Tyke C++ Build Script

This script provides automated build functionality for the Tyke C++ library,
supporting debug/release modes, static/shared libraries, and incremental builds.

Usage:
    python build.py [options]

Options:
    -h, --help      Show this help message and exit
    --debug         Build in debug mode (no optimization, debug symbols)
    --release       Build in release mode with O2 optimization (default)
    --static        Build static library only
    --shared        Build shared library only
    --all           Build both static and shared libraries (default)
    --clean         Clean build artifacts
    --force         Force rebuild (ignore incremental build cache)
    --verbose       Enable verbose output

Examples:
    python build.py                        # Build both libraries in release mode
    python build.py --debug --static       # Build static library in debug mode
    python build.py --release --shared     # Build shared library in release mode
    python build.py --clean                # Clean all build artifacts
    python build.py --force --all          # Force rebuild all libraries

Build Output:
    - Static library: build-{mode}/lib/static/tyke.a (Linux) or tyke.lib (Windows)
    - Shared library: build-{mode}/lib/shared/tyke.so (Linux) or tyke.dll (Windows)
"""

import os
import sys
import json
import hashlib
import subprocess
import argparse
import platform
import shutil
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Optional, Tuple


class BuildConfig:
    """Build configuration class."""
    
    def __init__(self, project_dir: Path):
        self.project_dir = project_dir
        self.source_dir = project_dir / "tyke"
        self.cache_dir = project_dir / ".build_cache"
        self.cache_file = self.cache_dir / "cpp_build_cache.json"
        
        self.system = platform.system().lower()
        self.is_windows = self.system == "windows"
        self.is_linux = self.system == "linux"
        
        self.cache_dir.mkdir(exist_ok=True)
    
    def get_build_dir(self, build_type: str) -> Path:
        return self.project_dir / f"build-{build_type}"


class IncrementalBuilder:
    """Incremental build support with file hash caching."""
    
    def __init__(self, config: BuildConfig):
        self.config = config
        self.cache = self._load_cache()
    
    def _load_cache(self) -> Dict:
        if self.config.cache_file.exists():
            try:
                with open(self.config.cache_file, 'r', encoding='utf-8') as f:
                    return json.load(f)
            except (json.JSONDecodeError, IOError):
                return {}
        return {}
    
    def _save_cache(self):
        with open(self.config.cache_file, 'w', encoding='utf-8') as f:
            json.dump(self.cache, f, indent=2)
    
    def _compute_file_hash(self, file_path: Path) -> str:
        hasher = hashlib.md5()
        with open(file_path, 'rb') as f:
            for chunk in iter(lambda: f.read(8192), b''):
                hasher.update(chunk)
        return hasher.hexdigest()
    
    def _compute_source_hash(self) -> str:
        hasher = hashlib.md5()
        extensions = ['.cpp', '.h', '.hpp']
        
        for ext in extensions:
            for file_path in sorted(self.config.source_dir.rglob(f'*{ext}')):
                if file_path.is_file():
                    file_hash = self._compute_file_hash(file_path)
                    relative_path = file_path.relative_to(self.config.source_dir)
                    hasher.update(str(relative_path).encode())
                    hasher.update(file_hash.encode())
        
        cmake_file = self.config.project_dir / "CMakeLists.txt"
        if cmake_file.exists():
            cmake_hash = self._compute_file_hash(cmake_file)
            hasher.update(b'CMakeLists.txt')
            hasher.update(cmake_hash.encode())
        
        tyke_cmake = self.config.source_dir / "CMakeLists.txt"
        if tyke_cmake.exists():
            tyke_cmake_hash = self._compute_file_hash(tyke_cmake)
            hasher.update(b'tyke/CMakeLists.txt')
            hasher.update(tyke_cmake_hash.encode())
        
        return hasher.hexdigest()
    
    def needs_rebuild(self, build_type: str, library_type: str) -> bool:
        cache_key = f"{build_type}_{library_type}"
        current_hash = self._compute_source_hash()
        cached_hash = self.cache.get(cache_key, {}).get('hash', '')
        
        if current_hash != cached_hash:
            self.cache[cache_key] = {
                'hash': current_hash,
                'last_build': datetime.now().isoformat()
            }
            return True
        return False
    
    def mark_built(self, build_type: str, library_type: str):
        cache_key = f"{build_type}_{library_type}"
        if cache_key not in self.cache:
            self.cache[cache_key] = {}
        self.cache[cache_key]['last_build'] = datetime.now().isoformat()
        self._save_cache()


class CMakeBuilder:
    """CMake-based build implementation."""
    
    def __init__(self, config: BuildConfig, incremental: IncrementalBuilder):
        self.config = config
        self.incremental = incremental
    
    def configure(self, build_type: str, build_shared: bool, build_static: bool) -> bool:
        build_dir = self.config.get_build_dir(build_type)
        build_dir.mkdir(exist_ok=True)
        
        cmake_args = [
            'cmake',
            '-B', str(build_dir),
            '-S', str(self.config.project_dir),
            f'-DCMAKE_BUILD_TYPE={build_type.capitalize()}',
        ]
        
        if build_shared and build_static:
            cmake_args.append('-DBUILD_BOTH_LIBS=ON')
        elif build_shared:
            cmake_args.append('-DBUILD_SHARED_LIBS=ON')
        else:
            cmake_args.append('-DBUILD_SHARED_LIBS=OFF')
        
        if self.config.is_windows:
            cmake_args.extend(['-G', 'Visual Studio 17 2022'])
        
        print(f"[CONFIG] Configuring CMake for {build_type} build...")
        print(f"  Build directory: {build_dir}")
        print(f"  Library type: {'shared' if build_shared else 'static'}")
        
        try:
            result = subprocess.run(cmake_args, capture_output=True, text=True)
            if result.returncode != 0:
                print(f"[ERROR] CMake configuration failed:")
                print(result.stderr)
                return False
            print("[CONFIG] CMake configuration successful")
            return True
        except FileNotFoundError:
            print("[ERROR] CMake not found. Please install CMake and add it to PATH.")
            return False
    
    def build(self, build_type: str, library_type: str) -> bool:
        build_dir = self.config.get_build_dir(build_type)
        
        if not build_dir.exists():
            print(f"[ERROR] Build directory not found: {build_dir}")
            print("[ERROR] Please run configure first or use --force")
            return False
        
        print(f"[BUILD] Building {library_type} library ({build_type})...")
        
        build_args = [
            'cmake', '--build', str(build_dir),
            '--config', build_type.capitalize(),
            '--parallel'
        ]
        
        try:
            result = subprocess.run(build_args, capture_output=True, text=True)
            if result.returncode != 0:
                print(f"[ERROR] Build failed:")
                print(result.stderr)
                return False
            
            self.incremental.mark_built(build_type, library_type)
            print(f"[BUILD] Build successful!")
            self._print_output_locations(build_dir, library_type)
            return True
        except Exception as e:
            print(f"[ERROR] Build failed with exception: {e}")
            return False
    
    def _print_output_locations(self, build_dir: Path, library_type: str):
        lib_dir = build_dir / "lib" / library_type
        print(f"  Output directory: {lib_dir}")
        
        if self.config.is_windows:
            if library_type == "static":
                print(f"  Static library: {lib_dir}/tyke.lib")
            else:
                print(f"  Shared library: {lib_dir}/tyke.dll")
                print(f"  Import library: {lib_dir}/tyke.lib")
        else:
            if library_type == "static":
                print(f"  Static library: {lib_dir}/libtyke.a")
            else:
                print(f"  Shared library: {lib_dir}/libtyke.so")
    
    def clean(self):
        print("[CLEAN] Cleaning build artifacts...")
        
        for build_dir in self.config.project_dir.glob("build-*"):
            if build_dir.is_dir():
                print(f"  Removing {build_dir}")
                shutil.rmtree(build_dir)
        
        if self.config.cache_file.exists():
            self.config.cache_file.unlink()
            print("  Build cache cleared")
        
        print("[CLEAN] Clean complete!")


def check_dependencies() -> bool:
    """Check if required dependencies are available."""
    print("[CHECK] Checking dependencies...")
    
    all_ok = True
    
    try:
        result = subprocess.run(['cmake', '--version'], capture_output=True, text=True)
        version = result.stdout.split('\n')[0] if result.stdout else 'unknown'
        print(f"  [OK] CMake: {version}")
    except FileNotFoundError:
        print("  [FAIL] CMake not found")
        all_ok = False
    
    try:
        if platform.system().lower() == 'windows':
            result = subprocess.run(['cl'], capture_output=True, text=True)
            print("  [OK] MSVC compiler found")
        else:
            result = subprocess.run(['g++', '--version'], capture_output=True, text=True)
            version = result.stdout.split('\n')[0] if result.stdout else 'unknown'
            print(f"  [OK] G++: {version}")
    except FileNotFoundError:
        print("  [FAIL] C++ compiler not found")
        all_ok = False
    
    return all_ok


def main():
    parser = argparse.ArgumentParser(
        description='Tyke C++ Build Script',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python build.py                        Build both libraries in release mode
  python build.py --debug --static       Build static library in debug mode
  python build.py --release --shared     Build shared library in release mode
  python build.py --clean                Clean all build artifacts
  python build.py --force --all          Force rebuild all libraries

Build Output:
  Static library: build-{mode}/lib/static/tyke.a (Linux) or tyke.lib (Windows)
  Shared library: build-{mode}/lib/shared/tyke.so (Linux) or tyke.dll (Windows)
        """
    )
    
    parser.add_argument('--debug', action='store_true', help='Build in debug mode')
    parser.add_argument('--release', action='store_true', help='Build in release mode (default)')
    parser.add_argument('--static', action='store_true', help='Build static library only')
    parser.add_argument('--shared', action='store_true', help='Build shared library only')
    parser.add_argument('--all', action='store_true', help='Build both static and shared libraries (default)')
    parser.add_argument('--clean', action='store_true', help='Clean build artifacts')
    parser.add_argument('--force', action='store_true', help='Force rebuild')
    parser.add_argument('--verbose', action='store_true', help='Enable verbose output')
    
    args = parser.parse_args()
    
    project_dir = Path(__file__).parent
    config = BuildConfig(project_dir)
    incremental = IncrementalBuilder(config)
    builder = CMakeBuilder(config, incremental)
    
    if args.clean:
        builder.clean()
        return 0
    
    if not check_dependencies():
        print("\n[ERROR] Dependency check failed. Please install missing dependencies.")
        return 1
    
    build_type = 'debug' if args.debug else 'release'
    
    build_static = args.static or (not args.shared and not args.static) or args.all
    build_shared = args.shared or (not args.shared and not args.static) or args.all
    
    if args.static and args.shared:
        build_static = True
        build_shared = True
    
    print(f"\n{'='*60}")
    print(f"Tyke C++ Build")
    print(f"{'='*60}")
    print(f"Build type: {build_type}")
    print(f"Static library: {'yes' if build_static else 'no'}")
    print(f"Shared library: {'yes' if build_shared else 'no'}")
    print(f"{'='*60}\n")
    
    success = True
    
    if build_static:
        library_type = 'static'
        if args.force or incremental.needs_rebuild(build_type, library_type):
            if not builder.configure(build_type, build_shared=False, build_static=True):
                success = False
            elif not builder.build(build_type, library_type):
                success = False
        else:
            print(f"[SKIP] No changes detected for {library_type} library")
    
    if build_shared and success:
        library_type = 'shared'
        if args.force or incremental.needs_rebuild(build_type, library_type):
            if not builder.configure(build_type, build_shared=True, build_static=False):
                success = False
            elif not builder.build(build_type, library_type):
                success = False
        else:
            print(f"[SKIP] No changes detected for {library_type} library")
    
    if success:
        print(f"\n{'='*60}")
        print("[SUCCESS] Build completed successfully!")
        print(f"{'='*60}")
        return 0
    else:
        print(f"\n{'='*60}")
        print("[FAILED] Build failed!")
        print(f"{'='*60}")
        return 1


if __name__ == '__main__':
    sys.exit(main())
