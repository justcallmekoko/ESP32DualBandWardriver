# === ESP32-C5 Auto Flasher Script By: AWOK ===

import sys
import subprocess
import os
import platform   # Placeholder for possible OS checks
import glob
import time
import shutil
import argparse

def version_tuple(v):
    return tuple(int(x) for x in v.split('.') if x.isdigit())
    
def ensure_esptool_version(min_version="5.0.1"):
    try:
        import esptool
        installed = esptool.__version__
    except Exception:
        installed = None

    if installed is None or version_tuple(installed) < version_tuple(min_version):
        print(f"Installing/upgrading esptool to >= {min_version} (current: {installed})")
        subprocess.check_call([
            sys.executable, "-m", "pip", "install",
            f"esptool>={min_version}", "--upgrade"
        ])

        # reload module after install
        import importlib
        import esptool
        importlib.reload(esptool)

    print(f"Using esptool version: {esptool.__version__}")

def ensure_package(pkg):
    try:
        __import__(pkg if pkg != 'gitpython' else 'git')
    except ImportError:
        print(f"Installing missing package: {pkg}")
        subprocess.check_call([sys.executable, '-m', 'pip', 'install', '--upgrade', pkg])

try:
    import serial.tools.list_ports
except ImportError:
    ensure_package('pyserial')
    
ensure_esptool_version("5.0.1")
import esptool

try:
    from colorama import Fore, Style
except ImportError:
    ensure_package('colorama')

# Dependency check and install if needed
REQUIRED_PACKAGES = [
    'pyserial',
    'esptool',
    'colorama'
]

def ensure_requirements():
    for pkg in REQUIRED_PACKAGES:
        ensure_package(pkg)
ensure_requirements()

# Finds the first file from a list of possible names in the bins folder
def find_file(name_options, bins_dir):
    for name in name_options:
        files = glob.glob(os.path.join(bins_dir, name))
        if files:
            return files[0]
    return None
    
def run_esptool(args, success_msg=None, fail_prefix="esptool failed"):
    """
    Runs esptool.main(args) but prevents esptool from terminating this script via sys.exit().
    Treats exit code 0 as success, non-zero as failure.
    """
    try:
        esptool.main(args)
        if success_msg:
            print(success_msg)
        return True
    except SystemExit as e:
        # esptool calls sys.exit(code). code==0 means success.
        code = e.code if isinstance(e.code, int) else 0
        if code == 0:
            if success_msg:
                print(success_msg)
            return True
        print(Fore.RED + f"{fail_prefix} (exit code {code})" + Style.RESET_ALL)
        return False
    except Exception as e:
        print(Fore.RED + f"{fail_prefix}: {e}" + Style.RESET_ALL)
        return False

def main():
    parser = argparse.ArgumentParser(description="ESP32-C5 Auto Flasher (bins subdir)")
    parser.parse_args()

    bins_dir = os.path.join(os.path.dirname(__file__), 'bins')
    if not os.path.isdir(bins_dir):
        print(Fore.RED + f"Bins directory not found: {bins_dir}\nPlease create a 'bins' folder with your .bin files." + Style.RESET_ALL)
        exit(1)

    # Logo and splash, both centered and purple
    terminal_width = shutil.get_terminal_size((100, 20)).columns
    def center(text): return text.center(terminal_width)
    logo_lines = [
        "                                            @@@@@@                              ",
        "                                @@@@@@@@  @@@@@ @@@@                            ",
        "                               @@@    @@@@@@@     @@@     @@@@@@@@              ",
        "                    @@@@@@@@@@@@@      @@@@@       @@@  @@@@    @@@             ",
        "                   @@@      @@@@@       @@@         @@@@@@       @@@            ",
        "                  @@@         @@@        @@         @@@@         @@@            ",
        "         @@@@@@@@@@@@          @@        @          @@@         @@@     @@      ",
        "        @@@     @@@@@           @@           @@@    @@          @@@@@@@@@@@@@@  ",
        "       @@@         @@@           @@          @@@               @@@@@        @@@@",
        "       @@@           @@                     @@@@              @@@            @@@",
        "      @@@@@            @@                   @@@@                             @@@",
        "   @@@@@@  @@                @@         @   @@@                            @@@@ ",
        "  @@@                         @@       @@         @                     @@@@@@  ",
        " @@@                           @@     @@@        @@            @@@@@@@@@@@@@@@  ",
        "@@@       @@@@      @          @@@@  @@@@@     @@@        @      @@@@  @@@ @@@  ",
        "@@@       @@@@@@@    @@@       @@@@@@@@@@@@@@@@@@@       @@        @@@ @@@      ",
        "@@@        @@@@@ @@@  @@@@@  @@@@ @@@@  @@@@@@@@@@      @@@@        @@@         ",
        "@@@@            @@@@@@@@@@@@@@@@  @@@   @@@ @@  @@@@@@@@@@@@         @@@        ",
        " @@@@          @@@@@@@@@@@@@@@@   @@@       @@   @@@@@@@@@@@@        @@@        ",
        "  @@@@@@     @@@@@   @@@     @@    @               @@  @@  @@@@    @@@@         ",
        "   @@@@@@@@@@@@@     @@@     @@                    @@  @@   @@@@@@@@@@@         ",
        "   @@@@@@@@@@@@      @@@     @@                   @@@  @     @@@@@@@@           ",
        "        @@  @@                                    @@@        @@  @@@            ",
        "        @   @@                                               @@  @@@            ",
        "           @@@                                                   @@@            ",
        "           @@@                                                                  ",
        ""
    ]
    splash_lines = [
        "--  ESP32 C5 Flasher --",
        "By AWOK",
        "Inspired from LordSkeletonMans ESP32 FZEasyFlasher",
        "Shout out to JCMK for the inspiration on setting up the C5",
        ""
    ]
    print(Fore.MAGENTA + "\n" + "\n".join(center(line) for line in logo_lines + splash_lines) + Style.RESET_ALL)

    # Wait for ESP32 device to show up as a new serial port
    existing_ports = set([port.device for port in serial.tools.list_ports.comports()])
    print(Fore.YELLOW + "Waiting for ESP32-C5 device to be connected..." + Style.RESET_ALL)
    while True:
        current_ports = set([port.device for port in serial.tools.list_ports.comports()])
        new_ports = current_ports - existing_ports
        if new_ports:
            serial_port = new_ports.pop()
            break
        time.sleep(0.5)
    print(Fore.GREEN + f"Detected ESP32-C5 on port: {serial_port}" + Style.RESET_ALL)

    # Find bin files for each firmware component
    bootloader = find_file(['bootloader.bin'], bins_dir)
    partitions = find_file(['partition-table.bin', 'partitions.bin'], bins_dir)
    ota_data = find_file(['ota_data_initial.bin'], bins_dir)

    # Main firmware: largest bin in the folder that's not bootloader, partition, or OTA
    all_bins = glob.glob(os.path.join(bins_dir, "*.bin"))
    exclude = {bootloader, partitions, ota_data}
    firmware_bins = [f for f in all_bins if f not in exclude and os.path.isfile(f)]
    if not firmware_bins:
        print(Fore.RED + "No application firmware .bin file found in the 'bins' folder!" + Style.RESET_ALL)
        exit(1)
    app_bin = max(firmware_bins, key=lambda f: os.path.getsize(f))

    # Print summary, ask for confirmation before flashing
    print(Fore.CYAN + f"\nBootloader:   {bootloader or 'NOT FOUND'}")
    print(f"Partitions:   {partitions or 'NOT FOUND'}")
    print(f"OTA Data:     {ota_data or 'NOT FOUND'}")
    print(f"App (main):   {app_bin}\n" + Style.RESET_ALL)
    if not (bootloader and partitions):
        print(Fore.RED + "Missing bootloader or partition table. Both are required for a complete flash!" + Style.RESET_ALL)
        exit(1)
    confirm = input(Fore.YELLOW + "Ready to flash these files to ESP32-C5? (y/N): " + Style.RESET_ALL)
    if confirm.strip().lower() != 'y':
        print("Aborting.")
        exit(0)
        
    # --- erase flash before flashing ---
    print(Fore.YELLOW + "Erasing ESP32-C5 flash (this may take a bit)..." + Style.RESET_ALL)

    erase_args = [
        '--chip', 'esp32c5',
        '--port', serial_port,
        '--baud', '921600',
        '--before', 'default_reset',
        '--after', 'hard_reset',
        'erase_flash'
    ]

    if not run_esptool(erase_args, success_msg=Fore.GREEN + "Erase complete!" + Style.RESET_ALL,
                      fail_prefix="Erase failed"):
        exit(1)

    # --- now do write_flash ---
    print(Fore.YELLOW + "Flashing ESP32-C5 with bootloader, partition table, and application..." + Style.RESET_ALL)

    write_args = [
        '--chip', 'esp32c5',
        '--port', serial_port,
        '--baud', '921600',
        '--before', 'default_reset',
        '--after', 'hard_reset',
        'write_flash', '-z',
        '0x2000', bootloader,
        '0x8000', partitions,
    ]
    if ota_data:
        write_args += ['0xd000', ota_data]
    write_args += ['0x10000', app_bin]

    if not run_esptool(write_args, success_msg=Fore.GREEN + "Flashing complete!" + Style.RESET_ALL,
                      fail_prefix="Flashing failed"):
        exit(1)

if __name__ == "__main__":
    main()