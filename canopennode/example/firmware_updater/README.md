# Firmware Updater Quickstart

This guide explains how to run the demo firmware update flow, how to drop the slave and master sources into your own project, how to obtain the firmware binary (`.bin`) from the Espressif build system, and how to drive the mock master uploader.

## 1. Overview of the two demo programs

- `main_firmware_update.c` acts as the Controller Area Network Open slave. It exposes the CiA 302 firmware download objects, validates metadata, simulates flash erases, and keeps the standard Controller Area Network Open tasks alive.
- `master_firmware_uploader.c` represents the Controller Area Network Open master. It loads a firmware image from disk, computes the cyclic redundancy check, and echoes the sequence of metadata/start/chunk/finalize commands that you would later translate into Service Data Object client calls.

Both files live in `canopennode/example/firmware_updater/` so you can copy them into any project that already depends on CANopenNode.

## 2. Integrating the slave demo into an existing Espressif Internet Development Framework project

1. Copy `firmware_updater/main_firmware_update.c` into your project’s `main/` directory (or create a dedicated component if you prefer).
2. In that file’s accompanying `CMakeLists.txt`, make sure the call to `idf_component_register` includes `REQUIRES canopennode` so the CANopen stack links correctly:
   ```cmake
   idf_component_register(SRCS "main_firmware_update.c" REQUIRES canopennode)
   ```
3. Configure your `sdkconfig` and hardware abstraction so the CAN driver, timer callbacks, and storage hooks match your board. The demo already logs every step over the default Universal Asynchronous Receiver Transmitter, so no additional logging wiring is required.
4. Build with the normal command:
   ```pwsh
   idf.py build
   ```

## 3. Getting the firmware `.bin`

After `idf.py build` finishes, the primary firmware image is emitted under the project’s `build/` folder. The exact file name matches your project name—for example `build/my_app.bin`. You can verify the path by inspecting `build/flasher_args.json` or running `idf.py size`.

That `.bin` file is what the master uploader expects when you test the Controller Area Network Open transfer.

## 4. Building and running the master uploader

1. From any desktop environment with a C compiler, build `master_firmware_uploader.c` (or compile it inside your preferred host application). Example with `gcc`:
   ```pwsh
   cd canopennode/example/firmware_updater
   gcc master_firmware_uploader.c -o master_firmware_uploader
   ```
2. Run the uploader, pointing it at the `.bin` produced by your Espressif build. The optional arguments are the target node identifier and the bank number you want to program:
   ```pwsh
   .\master_firmware_uploader ..\..\..\..\path\to\esp-idf-project\build\my_app.bin 10 1
   ```
   - `10` selects node identifier 10 for the slave.
   - `1` selects bank 1 (adjust if your bootloader uses a different scheme).

The uploader prints each stage (metadata write, start command, chunk streaming, finalize request). Replace the stubbed `send_*` helpers with real Service Data Object client calls when you are ready to talk to hardware.

## 5. Putting it all together

1. Flash the slave demo to your ESP32 board (`idf.py -p <PORT> flash monitor`) and keep the serial monitor open to watch the `[FW-DEMO]` logs.
2. Build and launch the master uploader with the `.bin` you just compiled.
3. Observe the slave’s console to confirm metadata validation, chunk receipts, and cyclic redundancy check verification. The master console should report the same stages.
4. Once you are satisfied, replace the simulated flash hooks in `main_firmware_update.c` with real partition writes and wire the uploader’s stub calls to the CANopen Service Data Object client API.

This flow gives you a complete loop—from generating a firmware image in an Espressif project, through staging it on the master, to observing the slave’s checks—before moving on to production hardware or bootloaders.

## 6. Notes for STMicroelectronics targets

The Controller Area Network Open logic shown here is portable. When you move from Espressif hardware to an STMicroelectronics board, only the hardware abstraction layers change:

- **Slave side**: keep `main_firmware_update.c` but replace the CAN driver glue, timer callbacks, and flash write helpers with equivalents from the STMicroelectronics Hardware Abstraction Layer or Low-Layer drivers. The object dictionary entries, metadata validation, and state machine logic remain identical.
- **Master side**: the uploader already runs on a desktop or any generic controller, so nothing is ST-specific. If you later port the master onto an STMicroelectronics microcontroller, reuse the same `fw_*` helpers and provide a CAN driver plus Service Data Object client implementation for that platform.
- **Firmware artifact**: for Espressif you copy `build/my_app.bin`. For STMicroelectronics you would instead grab the binary or hexadecimal image produced by STM32CubeIDE or your preferred build system. Feed that file to the uploader the same way—the metadata structure and transfer commands stay the same.

In short, the features and flow do not change between Espressif and STMicroelectronics targets; only the board-level drivers and flash access layers need to be swapped when you port the code.
