#!/usr/bin/python3
from pathlib import Path
import argparse
import subprocess
import shutil
import os

if __name__ == "__main__":

    parser = argparse.ArgumentParser(description="Compile CP with SymCC")
    parser.add_argument("--cp-root", required=True, type=str, help="Path to the CP root")
    args = parser.parse_args()

    out_dir_cp = Path("/out")
    out_dir_crs = Path(args.cp_root) / "out"

    cc_wrapper_name, cxx_wrapper_name = "cc-wrapper", "cxx-wrapper"
    cc_wrapper_cp = out_dir_cp / cc_wrapper_name
    cxx_wrapper_cp = out_dir_cp / cxx_wrapper_name
    cc_wrapper_crs = out_dir_crs / cc_wrapper_name
    cxx_wrapper_crs = out_dir_crs / cxx_wrapper_name
    clang_wrapper_crs = Path("clang-wrapper/build/clang-wrapper") 

    # Compile the wrappers
    subprocess.run("./build.sh", shell=True, cwd="clang-wrapper").check_returncode()
    
    # Copy the wrappers to the out directory
    shutil.copy(clang_wrapper_crs, cc_wrapper_crs)
    shutil.copy(clang_wrapper_crs, cxx_wrapper_crs)

    # Run the container build using the clang wrapper
    docker_extra_args = f"-e CC={cc_wrapper_cp} -e CXX={cxx_wrapper_cp}"
    subprocess.run(f"DOCKER_EXTRA_ARGS=\"{docker_extra_args}\" ./run.sh build", shell=True, cwd=args.cp_root).check_returncode()

    # Now, we have the compiler command lines as well as the filesystem snapshot at the point of compiler invocation
    # We can replay this command line with clang replaced to SymCC and vice versa
    # To ensure the ordering of dependencies, we read ts.txt in each of the command lines and execute them in increasing order

    hash_to_timestamp = dict()
    compile_commands = out_dir_crs / "compile_commands"
    for cc_hash in os.listdir(compile_commands):
        with open(compile_commands / cc_hash / "ts.txt", "r") as f:
            ts = int(f.read().replace("\n", ""))
            hash_to_timestamp[cc_hash] = ts
    
    ordered_hashes = sorted(hash_to_timestamp, key=hash_to_timestamp.get)

    # now 'replay' each of them
    orig_cwd = os.getcwd()
    for cc_hash in ordered_hashes:
        fs_snapshot = compile_commands / cc_hash / "fs_snapshot"
        with open(compile_commands / cc_hash / "cmd.txt", "r") as f:
            args = f.read().replace("\n", "").split(" ")
        with open(compile_commands / cc_hash / "cwd.txt", "r") as f:
            cp_cwd = f.read().replace("\n", "")
            if cp_cwd.startswith("/"):
                cp_cwd = cp_cwd[1:]
            crs_cwd = fs_snapshot / cp_cwd

        if "cc" in args[0]:
            args[0] = "clang"
        elif "cxx" in args[0]:
            args[0] = "clang++"
        else:
            raise ValueError(f"Unrecognized compiler: {args[0]}")
        
        os.chdir(crs_cwd)
        subprocess.run(args).check_returncode()
        os.chdir(orig_cwd)