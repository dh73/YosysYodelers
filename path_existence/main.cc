#include "kernel/celltypes.h"
#include "kernel/sigtools.h"
#include "kernel/yosys.h"

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

struct LtpWorker {
    RTLIL::Design *design;
    RTLIL::Module *module;
    SigMap sigmap;

    dict<SigBit, tuple<int, SigBit, Cell *>> bits;
    dict<SigBit, dict<SigBit, Cell *>> bit2bits;
    dict<SigBit, tuple<SigBit, Cell *>> bit2ff;

    SigBit from_bit;
    SigBit* to_bit = nullptr;

    pool<SigBit> busy;
    bool path_found;
    std::vector<SigBit> path;
    std::vector<std::pair<SigBit, Cell *>> path_with_cells;

    LtpWorker(RTLIL::Module *module, SigBit from, SigBit* to = nullptr)
    : design(module->design), module(module), sigmap(module), from_bit(from),
      to_bit(to), path_found(false) {
    CellTypes ff_celltypes;

    for (auto wire : module->wires()) {
        for (auto bit : sigmap(wire)) {
            bits[bit] = tuple<int, SigBit, Cell *>(-1, State::Sx, nullptr);
        }
    }

    for (auto cell : module->cells()) {
        pool<SigBit> src_bits, dst_bits;

        for (auto &conn : cell->connections()) {
            for (auto bit : sigmap(conn.second)) {
                if (cell->input(conn.first))
                    src_bits.insert(bit);
                if (cell->output(conn.first))
                    dst_bits.insert(bit);
            }
        }

        for (auto s : src_bits) {
            for (auto d : dst_bits) {
                bit2bits[s][d] = cell;
            }
        }
    }

    if (to_bit) {
        *to_bit = sigmap(*to_bit);
    }
}

bool dfs(SigBit current_bit) {
    log_debug("DFS at bit: %s\n", log_signal(current_bit));

    if (current_bit == from_bit) {
        path.push_back(current_bit);
    }

    if (to_bit && sigmap(current_bit) == sigmap(*to_bit)) {
        log_debug("Reached to_bit: %s\n", log_signal(*to_bit));
        path.push_back(current_bit);
        return true;
    }

    if (busy.count(current_bit) > 0) {
        log_debug("Loop detected at bit: %s\n", log_signal(current_bit));
        return false;
    }
    busy.insert(current_bit);

    bool path_exists = false;
    if (bit2bits.count(current_bit)) {
        for (auto &it : bit2bits.at(current_bit)) {
            if (dfs(it.first)) {
                path_with_cells.push_back({current_bit, it.second});
                path_exists = true;
                if (to_bit) {
                    break;
                }
            }
        }
    }

    if (!to_bit || path_exists) {
        path.push_back(current_bit);
    }

    busy.erase(current_bit);
    return path_exists;
}

void run() {
    log_debug("Running DFS from %s\n", log_signal(from_bit));
    path_found = dfs(from_bit);

    if (!path.empty()) {
        std::reverse(path.begin(), path.end());

        size_t path_size = path.size();
        if (path.front() == path.back() && path_size > 1) {
            path_size--;
        }

        for (size_t i = 0; i < path_size; ++i) {
            Cell *via = nullptr;
            if (!path_with_cells.empty() && i < path_with_cells.size()) {
                via = path_with_cells[i].second;
            }
            log("%5zu: %s (via %s)\n", i, log_signal(path[i]),
                via ? log_id(via) : "null");
        }
        log("\n");
    } else {
        if (to_bit) {
            log_warning("No path found from %s to %s.\n", log_signal(from_bit),
                        log_signal(*to_bit));
        } else {
            log_warning("No paths found from %s.\n", log_signal(from_bit));
        }
    }

}
};

struct LtpPass : public Pass {
    LtpPass() : Pass("path", "find a path from 'from' to 'to'") {}

    void help() override {
        log("\n");
        log("    path -from <signal> -to <signal>\n");
        log("\n");
        log("This command finds a path from a 'from' signal to a 'to' signal in the design.\n");
        log("\n");
    }

    void execute(std::vector<std::string> args, RTLIL::Design *design) override {
        log_header(design, "Executing path pass (find path from 'from' to 'to').\n");

        std::string from_name, to_name;
        bool from_set = false, to_set = false;
        bool schematic = false;

        size_t argidx;
        for (argidx = 1; argidx < args.size(); argidx++) {
            if (args[argidx] == "-from" && argidx + 1 < args.size()) {
                from_name = args[++argidx];
                from_set = true;
            } else if (args[argidx] == "-to" && argidx + 1 < args.size()) {
                to_name = args[++argidx];
                to_set = true;
            }
            if (args[argidx] == "-schematic") {
                schematic = true;
                continue;
            }
        }

        if (!from_set) {
            log_error("The '-from' argument must be specified.\n");
            return;
        }

        extra_args(args, argidx, design);

        for (Module *module : design->selected_modules()) {
            SigSpec from_sig;
            if (!SigSpec::parse(from_sig, module, from_name) || from_sig.empty() || from_sig.size() != 1) {
                log_error("The 'from' must be a single-bit signal.\n");
                return;
            }

	    SigMap sigmap(module);
	    SigBit from_bit = sigmap(from_sig.as_bit());

            SigBit* to_bit_ptr = nullptr;
            SigBit to_bit;

            if (to_set) {
                SigSpec to_sig;
                if (!SigSpec::parse(to_sig, module, to_name) || to_sig.empty() || to_sig.size() != 1) {
                    log_warning("The 'to' must be a single-bit signal.\n");
		    log("to_size = %d\n", to_sig.size());
                    return;
                }
                to_bit = sigmap(to_sig.as_bit());
                to_bit_ptr = &to_bit;
            }

            log_debug("Processing module: %s\n", log_id(module));
	    log_debug("Resolved from_bit: %s, to_bit: %s\n", log_signal(from_bit), to_bit_ptr ? log_signal(*to_bit_ptr) : "null");

            LtpWorker worker(module, from_bit, to_bit_ptr);
            worker.run();

            if (schematic && worker.path_found) {
                RTLIL::Selection path_selection(false);

                for (const auto &element : worker.path_with_cells) {
                    Cell *cell = element.second;
                    SigBit bit = element.first;
                    if (cell) {
                        path_selection.select(module, cell);
                    }
                    if (bit.wire) {
                        path_selection.select(module, bit.wire);
                    }
                }

                Pass::call_on_selection(module->design, path_selection, "show");
            }
        }
    }
} LtpPass;
PRIVATE_NAMESPACE_END
