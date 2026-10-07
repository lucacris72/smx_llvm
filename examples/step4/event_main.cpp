// Reuse the hardware driver's RAM dump and command-line helpers unchanged.
#define main hardware_driver_main
#include "sim_main.cpp"
#undef main

int main(int argc, char **argv) {
    VerilatedContext context;
    context.commandArgs(argc, argv);
    Vtb_top top{&context};
    while (!context.gotFinish()) {
        top.eval();
        if (context.gotFinish()) break;
        if (!top.eventsPending()) {
            std::cerr << "No pending event before firmware exit\n";
            return 1;
        }
        context.time(top.nextTimeSlot());
    }
    top.final();
    dump_ram_to_bin(&top, resolve_dump_path("memdump=", "sim_ram.bin"));
    dump_ram_to_hex(&top, resolve_dump_path("memdump_hex=", "sim_ram.hex"));
    return 0;
}
