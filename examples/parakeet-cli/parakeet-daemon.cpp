// Clyra: long-lived Parakeet worker. Loads the model once, then serves
// "<id>\t<wav path>" requests from stdin, answering "<id>\t<text>" on stdout.
// Removes the ~1s model load that the one-shot parakeet-cli pays per call.
#include "parakeet.h"
#include "common-whisper.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

static void cb_log_disable(enum ggml_log_level, const char *, void *) {}

int main(int argc, char ** argv) {
    ggml_backend_load_all();
    parakeet_log_set(cb_log_disable, NULL);

    std::string model;
    int32_t n_threads = std::min(4, (int32_t) std::thread::hardware_concurrency());
    bool use_gpu = false;
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if ((arg == "-m" || arg == "--model") && i + 1 < argc) model = argv[++i];
        else if ((arg == "-t" || arg == "--threads") && i + 1 < argc) n_threads = std::stoi(argv[++i]);
        else if (arg == "--gpu") use_gpu = true;
    }
    if (model.empty()) { fprintf(stderr, "usage: parakeet-daemon -m model [-t N] [--gpu]\n"); return 1; }

    struct parakeet_context_params ctx_params = parakeet_context_default_params();
    ctx_params.use_gpu = use_gpu;
    struct parakeet_context * pctx = parakeet_init_from_file_with_params(model.c_str(), ctx_params);
    if (!pctx) { fprintf(stderr, "error: failed to load model '%s'\n", model.c_str()); return 1; }

    auto transcribe = [&](const std::vector<float> & pcm) -> std::string {
        struct parakeet_full_params fp = parakeet_full_default_params(PARAKEET_SAMPLING_GREEDY);
        fp.n_threads = n_threads;
        if (parakeet_full(pctx, fp, pcm.data(), pcm.size()) != 0) return "";
        std::string out;
        const int n = parakeet_full_n_segments(pctx);
        for (int i = 0; i < n; i++) {
            const char * t = parakeet_full_get_segment_text(pctx, i);
            if (t) { if (!out.empty()) out += ' '; out += t; }
        }
        for (auto & c : out) if (c == '\n' || c == '\t' || c == '\r') c = ' ';
        return out;
    };

    // Prime allocations/caches so the first real utterance is already fast.
    transcribe(std::vector<float>(16000, 0.0f));

    printf("READY\n");
    fflush(stdout);

    std::string line;
    while (std::getline(std::cin, line)) {
        const size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;
        const std::string id = line.substr(0, tab);
        const std::string path = line.substr(tab + 1);
        std::vector<float> pcm;
        std::vector<std::vector<float>> stereo;
        std::string text;
        if (read_audio_data(path.c_str(), pcm, stereo, false) && !pcm.empty()) text = transcribe(pcm);
        printf("%s\t%s\n", id.c_str(), text.c_str());
        fflush(stdout);
    }
    parakeet_free(pctx);
    return 0;
}
