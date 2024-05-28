#include "kernel/yosys.h"
#include "kernel/celltypes.h"
#include "kernel/sigtools.h"
#include <algorithm>

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

struct GraphIsomorphismWorker {
    RTLIL::Design *design;
    RTLIL::Module *module;
    SigMap sigmap;
    dict<SigBit, tuple<int, SigBit, Cell*>> bits;
    dict<SigBit, dict<SigBit, Cell*>> bit2bits;
    vector<SigBit> topo_order;
    vector<string> path_a;
    vector<string> path_b;
    SigBit from_bit_a;
    SigBit from_bit_b;
    SigBit to_bit_a;
    SigBit to_bit_b;

    GraphIsomorphismWorker(RTLIL::Module *module, SigBit from_bit_a, SigBit from_bit_b, SigBit to_bit_a, SigBit to_bit_b)
        : design(module->design), module(module), sigmap(module),
          from_bit_a(from_bit_a), from_bit_b(from_bit_b), to_bit_a(to_bit_a), to_bit_b(to_bit_b) {
        for (auto wire : module->selected_wires())
            for (auto bit : sigmap(wire))
                bits[bit] = tuple<int, SigBit, Cell*>(-1, State::Sx, nullptr);

        for (auto cell : module->selected_cells()) {
            pool<SigBit> src_bits, dst_bits;

            for (auto &conn : cell->connections())
                for (auto bit : sigmap(conn.second)) {
                    if (cell->input(conn.first))
                        src_bits.insert(bit);
                    if (cell->output(conn.first))
                        dst_bits.insert(bit);
                }

            for (auto s : src_bits)
                for (auto d : dst_bits)
                    bit2bits[s][d] = cell;
        }
    }

    void topological_sort() {
        pool<SigBit> visited;
        pool<SigBit> processed;

        std::function<void(SigBit)> dfs = [&](SigBit bit) {
            if (processed.count(bit) > 0)
                return;

            if (visited.count(bit) > 0)
                log_cmd_error("Found a cycle in the circuit!\n");

            visited.insert(bit);

            if (bit2bits.count(bit) > 0) {
                for (auto &p : bit2bits.at(bit))
                    dfs(p.first);
            }

            processed.insert(bit);
            topo_order.push_back(bit);
        };

        for (auto &it : bits)
            dfs(it.first);

        std::reverse(topo_order.begin(), topo_order.end());
    }

    bool runner(SigBit bit, Cell *via, vector<string> &path) {
        if ((bit == to_bit_a && path == path_a) || (bit == to_bit_b && path == path_b))
            return true;

        path.push_back(stringf("%s (via %s)", log_signal(bit), via ? log_id(via) : "input"));

        if (bit2bits.count(bit) > 0) {
            for (auto &it : bit2bits.at(bit))
                if (runner(it.first, it.second, path))
                    return true;
        }

        path.pop_back();
        return false;
    }

    void run() {
        bool isomorphic = true;

        topological_sort();

        for (auto bit : topo_order) {
            if (bit == from_bit_a)
                if (!runner(bit, nullptr, path_a)) {
                    log("No path found from %s to %s.\n", log_signal(from_bit_a), log_signal(to_bit_a));
                    isomorphic = false;
                    break;
                }

            if (bit == from_bit_b)
                if (!runner(bit, nullptr, path_b)) {
                    log("No path found from %s to %s.\n", log_signal(from_bit_b), log_signal(to_bit_b));
                    isomorphic = false;
                    break;
                }
        }

        if (isomorphic && path_a.size() == path_b.size()) {
            log("Paths are isomorphic:\n");
            for (size_t i = 0; i < path_a.size(); i++)
                log("  %s %s\n", path_a[i].c_str(), path_b[i].c_str());
        } else {
            log("Paths are not isomorphic:\n");
            log("Path A:\n");
            for (const auto &p : path_a)
                log("  %s\n", p.c_str());
            log("Path B:\n");
            for (const auto &p : path_b)
                log("  %s\n", p.c_str());
        }
    }
};

struct GraphIsomorphismPass : public Pass {
    GraphIsomorphismPass() : Pass("graphiso", "check graph isomorphism between two paths") {}

    void help() override {
        log("\n");
        log("    graphiso -from_a <signal> -from_b <signal> -to_a <signal> -to_b <signal> [selection]\n");
        log("\n");
        log("This command checks for graph isomorphism between two paths in the design.\n");
        log("It starts from the specified '-from_a' and '-from_b' signals and tries to reach\n");
        log("the '-to_a' and '-to_b' signals respectively. It compares the paths traversed\n");
        log("and reports whether they are isomorphic or not.\n");
        log("\n");
    }

    void execute(std::vector<std::string> args, RTLIL::Design *design) override {
        log_header(design, "Executing GRAPHISO pass (graph isomorphism check).\n");

        string from_str_a, from_str_b, to_str_a, to_str_b;
        size_t argidx;
        for (argidx = 1; argidx < args.size(); argidx++) {
            if (args[argidx] == "-from_a" && argidx+1 < args.size()) {
                from_str_a = args[++argidx];
                continue;
            }
            if (args[argidx] == "-from_b" && argidx+1 < args.size()) {
                from_str_b = args[++argidx];
                continue;
            }
            if (args[argidx] == "-to_a" && argidx+1 < args.size()) {
                to_str_a = args[++argidx];
                continue;
            }
            if (args[argidx] == "-to_b" && argidx+1 < args.size()) {
                to_str_b = args[++argidx];
                continue;
            }
            break;
        }

        extra_args(args, argidx, design);

        if (from_str_a.empty())
            log_cmd_error("'-from_a' argument is required.\n");

        if (from_str_b.empty())
            log_cmd_error("'-from_b' argument is required.\n");

        if (to_str_a.empty())
            log_cmd_error("'-to_a' argument is required.\n");

        if (to_str_b.empty())
            log_cmd_error("'-to_b' argument is required.\n");

        for (Module *module : design->selected_modules()) {
            if (module->has_processes_warn())
                continue;

            SigSpec from_sig_a, from_sig_b, to_sig_a, to_sig_b;

            if (!SigSpec::parse(from_sig_a, module, from_str_a) || from_sig_a.empty() || from_sig_a.size() != 1)
                log_cmd_error("The '-from_a' signal '%s' must be a single-bit signal in module '%s'.\n", from_str_a.c_str(), log_id(module));

            if (!SigSpec::parse(from_sig_b, module, from_str_b) || from_sig_b.empty() || from_sig_b.size() != 1)
                log_cmd_error("The '-from_b' signal '%s' must be a single-bit signal in module '%s'.\n", from_str_b.c_str(), log_id(module));

            if (!SigSpec::parse(to_sig_a, module, to_str_a) || to_sig_a.empty() || to_sig_a.size() != 1)
                log_cmd_error("The '-to_a' signal '%s' must be a single-bit signal in module '%s'.\n", to_str_a.c_str(), log_id(module));

            if (!SigSpec::parse(to_sig_b, module, to_str_b) || to_sig_b.empty() || to_sig_b.size() != 1)
                log_cmd_error("The '-to_b' signal '%s' must be a single-bit signal in module '%s'.\n", to_str_b.c_str(), log_id(module));

            SigMap sigmap(module);
            SigBit from_bit_a = sigmap(from_sig_a.as_bit());
            SigBit from_bit_b = sigmap(from_sig_b.as_bit());
            SigBit to_bit_a = sigmap(to_sig_a.as_bit());
            SigBit to_bit_b = sigmap(to_sig_b.as_bit());

            if (from_bit_a.wire == nullptr || from_bit_a.data == RTLIL::State::Sx)
                log_cmd_error("The '-from_a' signal '%s' does not exist or is not a valid single-bit signal in module '%s'.\n", from_str_a.c_str(), log_id(module));

            if (from_bit_b.wire == nullptr || from_bit_b.data == RTLIL::State::Sx)
                log_cmd_error("The '-from_b' signal '%s' does not exist or is not a valid single-bit signal in module '%s'.\n", from_str_b.c_str(), log_id(module));

            if (to_bit_a.wire == nullptr || to_bit_a.data == RTLIL::State::Sx)
                log_cmd_error("The '-to_a' signal '%s' does not exist or is not a valid single-bit signal in module '%s'.\n", to_str_a.c_str(), log_id(module));

            if (to_bit_b.wire == nullptr || to_bit_b.data == RTLIL::State::Sx)
                log_cmd_error("The '-to_b' signal '%s' does not exist or is not a valid single-bit signal in module '%s'.\n", to_str_b.c_str(), log_id(module));

            GraphIsomorphismWorker worker(module, from_bit_a, from_bit_b, to_bit_a, to_bit_b);
            worker.run();
        }
    }
} GraphIsomorphismPass;

PRIVATE_NAMESPACE_END
