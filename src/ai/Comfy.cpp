#include "ai/Comfy.h"
#include "ai/Net.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <random>
#include <thread>

namespace sx::comfy {

static std::atomic<bool> gCancel{false};
void CancelAll() { gCancel = true; }

namespace {
std::string UrlEncode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out.push_back((char)c);
        else { out.push_back('%'); out.push_back(hex[c >> 4]); out.push_back(hex[c & 15]); }
    }
    return out;
}

std::string Lower(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

// Error text of a ComfyUI JSON answer ({"error": {...}, "node_errors": {...}}).
std::string ErrorText(const Json& j) {
    std::string e = j["error"]["message"].S("");
    if (e.empty()) e = j["error"].S("");
    for (auto& [id, ne] : j["node_errors"].Items()) {
        for (auto& x : ne["errors"].Elements()) {
            e += "\n" + ne["class_type"].S("nœud") + " #" + id + " : " + x["message"].S("") + " " + x["details"].S("");
        }
    }
    return e.empty() ? "erreur ComfyUI" : e;
}

Json Node(const char* cls, const char* title) {
    Json n = Json::Object();
    n["class_type"] = cls;
    Json m = Json::Object();
    m["title"] = title;
    n["_meta"] = m;
    n["inputs"] = Json::Object();
    return n;
}

Json Link(const char* node, int output) {
    Json a = Json::Array();
    a.Push(node);
    a.Push(output);
    return a;
}
}  // namespace

Json BuiltinWorkflow(const Job& job, bool img2img) {
    Json w = Json::Object();
    Json ckpt = Node("CheckpointLoaderSimple", "Checkpoint");
    ckpt["inputs"]["ckpt_name"] = job.checkpoint;
    w["1"] = ckpt;
    Json pos = Node("CLIPTextEncode", "Prompt");
    pos["inputs"]["text"] = "{prompt}";
    pos["inputs"]["clip"] = Link("1", 1);
    w["2"] = pos;
    Json neg = Node("CLIPTextEncode", "Negative prompt");
    neg["inputs"]["text"] = "{negative}";
    neg["inputs"]["clip"] = Link("1", 1);
    w["3"] = neg;
    if (img2img) {
        Json load = Node("LoadImage", "Image");
        load["inputs"]["image"] = "input.png";
        w["4"] = load;
        Json enc = Node("VAEEncode", "Encode");
        enc["inputs"]["pixels"] = Link("4", 0);
        enc["inputs"]["vae"] = Link("1", 2);
        w["5"] = enc;
    } else {
        Json lat = Node("EmptyLatentImage", "Latent");
        lat["inputs"]["width"] = job.width;
        lat["inputs"]["height"] = job.height;
        lat["inputs"]["batch_size"] = 1;
        w["5"] = lat;
    }
    Json ks = Node("KSampler", "Sampler");
    ks["inputs"]["model"] = Link("1", 0);
    ks["inputs"]["positive"] = Link("2", 0);
    ks["inputs"]["negative"] = Link("3", 0);
    ks["inputs"]["latent_image"] = Link("5", 0);
    ks["inputs"]["seed"] = 0;
    ks["inputs"]["steps"] = job.steps;
    ks["inputs"]["cfg"] = 6.5;
    ks["inputs"]["sampler_name"] = "euler";
    ks["inputs"]["scheduler"] = "normal";
    ks["inputs"]["denoise"] = img2img ? std::round(job.denoise * 1000.0) / 1000.0 : 1.0;
    w["6"] = ks;
    Json dec = Node("VAEDecode", "Decode");
    dec["inputs"]["samples"] = Link("6", 0);
    dec["inputs"]["vae"] = Link("1", 2);
    w["7"] = dec;
    Json save = Node("SaveImage", "Save");
    save["inputs"]["images"] = Link("7", 0);
    save["inputs"]["filename_prefix"] = "Shaderixel";
    w["8"] = save;
    return w;
}

std::string ApplyJob(Json& w, const Job& job, const std::string& uploadedImage, u64 seed) {
    int prompts = 0, seeds = 0, sizes = 0, images = 0;
    std::vector<std::string> ids;
    for (auto& [id, n] : w.Items()) ids.push_back(id);
    for (const std::string& id : ids) {
        Json& n = w[id];
        if (!n.IsObject()) continue;
        std::string cls = n["class_type"].S("");
        std::string title = Lower(n["_meta"]["title"].S(""));
        Json& in = n["inputs"];
        auto isLiteral = [&](const char* k) { return in.Has(k) && !in[k].IsArray(); };
        if (cls.rfind("CLIPTextEncode", 0) == 0) {
            for (const char* k : {"text", "text_g", "text_l"}) {
                if (!isLiteral(k) || in[k].GetType() != Json::Type::String) continue;
                std::string t = in[k].S();
                bool hasP = t.find("{prompt}") != std::string::npos, hasN = t.find("{negative}") != std::string::npos;
                if (hasP || hasN) {
                    for (size_t at; (at = t.find("{prompt}")) != std::string::npos;) t.replace(at, 8, job.prompt);
                    for (size_t at; (at = t.find("{negative}")) != std::string::npos;) t.replace(at, 10, job.negative);
                    in[k] = t;
                    ++prompts;
                } else if (title.find("neg") != std::string::npos) {
                    if (!job.negative.empty()) in[k] = job.negative;
                } else {
                    in[k] = job.prompt;
                    ++prompts;
                }
            }
        }
        if ((cls == "KSampler" || cls == "KSamplerSelect" || cls == "SamplerCustom") && isLiteral("seed")) { in["seed"] = (double)seed; ++seeds; }
        if ((cls == "KSamplerAdvanced" || cls == "RandomNoise" || cls == "SamplerCustom") && isLiteral("noise_seed")) { in["noise_seed"] = (double)seed; ++seeds; }
        if (cls.find("EmptyLatentImage") != std::string::npos || cls.rfind("Empty", 0) == 0) {
            if (isLiteral("width") && isLiteral("height")) {
                in["width"] = job.width;
                in["height"] = job.height;
                ++sizes;
            }
        }
        if (cls == "LoadImage" && !uploadedImage.empty() && isLiteral("image")) { in["image"] = uploadedImage; ++images; }
    }
    char buf[160];
    snprintf(buf, sizeof(buf), "%d prompt(s), %d graine(s), %d taille(s), %d image(s)", prompts, seeds, sizes, images);
    return buf;
}

bool Run(const Job& job, std::vector<u8>& png, std::string& error, const std::function<void(const std::string&)>& progress) {
    auto say = [&](const std::string& s) { if (progress) progress(s); };
    std::string ep = job.endpoint;
    while (!ep.empty() && ep.back() == '/') ep.pop_back();
    Json w;
    if (!job.workflow.empty()) {
        std::string perr;
        if (!Json::Parse(job.workflow, w, &perr) || !w.IsObject()) { error = "workflow illisible : " + perr; return false; }
        if (w.Has("nodes") && w.Has("links")) { error = "workflow au format interface : dans ComfyUI, utilisez « Save (API format) »"; return false; }
    } else {
        w = BuiltinWorkflow(job, !job.inputPng.empty());
    }
    // Input image upload.
    std::string uploaded;
    if (!job.inputPng.empty()) {
        say("Envoi de l'image...");
        std::string boundary = "----ShaderixelComfy" + net::RandomToken(8);
        std::string body;
        body += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"image\"; filename=\"shaderixel_input.png\"\r\nContent-Type: image/png\r\n\r\n";
        body.append((const char*)job.inputPng.data(), job.inputPng.size());
        body += "\r\n--" + boundary + "\r\nContent-Disposition: form-data; name=\"overwrite\"\r\n\r\ntrue\r\n";
        body += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"type\"\r\n\r\ninput\r\n";
        body += "--" + boundary + "--\r\n";
        auto r = net::Http("POST", ep + "/upload/image", {{"Content-Type", "multipart/form-data; boundary=" + boundary}}, body, 60000);
        Json j;
        if (!r.error.empty()) { error = "ComfyUI injoignable (" + ep + ") : " + r.error; return false; }
        if (r.status != 200 || !Json::Parse(r.body, j)) { error = "envoi de l'image refusé (HTTP " + std::to_string(r.status) + ")"; return false; }
        uploaded = j["name"].S("");
        if (!j["subfolder"].S("").empty()) uploaded = j["subfolder"].S("") + "/" + uploaded;
    }
    std::mt19937_64 rng(std::random_device{}());
    std::string summary = ApplyJob(w, job, uploaded, rng() % 1000000000000ull);
    Json req = Json::Object();
    req["prompt"] = w;
    req["client_id"] = "shaderixel";
    say("Mise en file (" + summary + ")...");
    auto r = net::Http("POST", ep + "/prompt", {{"Content-Type", "application/json"}}, req.Dump(0), 30000);
    Json j;
    if (!r.error.empty()) { error = "ComfyUI injoignable (" + ep + ") : " + r.error; return false; }
    if (!Json::Parse(r.body, j)) { error = "réponse invalide (HTTP " + std::to_string(r.status) + ")"; return false; }
    if (r.status != 200 || j.Has("error")) { error = ErrorText(j); return false; }
    std::string id = j["prompt_id"].S("");
    if (id.empty()) { error = "pas d'identifiant de tâche"; return false; }
    // Wait for the result.
    auto start = std::chrono::steady_clock::now();
    Json outputs;
    for (;;) {
        if (std::chrono::steady_clock::now() - start > std::chrono::minutes(15)) { error = "délai dépassé (15 min)"; return false; }
        if (gCancel) { error = "annulé"; return false; }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        auto h = net::Http("GET", ep + "/history/" + UrlEncode(id), {}, "", 15000);
        Json hj;
        if (!h.error.empty() || !Json::Parse(h.body, hj) || !hj.Has(id)) continue;
        const Json& e = hj[id];
        if (e["status"]["status_str"].S("") == "error") {
            std::string msg = "exécution en erreur";
            for (auto& m : e["status"]["messages"].Elements())
                if (m.IsArray() && m.Size() > 1 && m.At(0).S("") == "execution_error") msg = m.At(1)["exception_message"].S(msg);
            error = msg;
            return false;
        }
        if (e["outputs"].Size() > 0 && (e["status"]["completed"].B(true))) { outputs = e["outputs"]; break; }
        say("Génération en cours...");
    }
    // First saved image (outputs of SaveImage first, else any preview).
    Json found;
    for (const char* wanted : {"output", ""})
        for (auto& [nid, o] : outputs.Items())
            for (auto& img : o["images"].Elements())
                if (found.IsNull() && (!*wanted || img["type"].S("") == wanted)) found = img;
    if (found.IsNull()) { error = "le workflow n'a produit aucune image (nœud SaveImage ?)"; return false; }
    say("Téléchargement...");
    auto v = net::Http("GET", ep + "/view?filename=" + UrlEncode(found["filename"].S("")) + "&subfolder=" + UrlEncode(found["subfolder"].S("")) +
                                  "&type=" + UrlEncode(found["type"].S("output")),
                       {}, "", 60000);
    if (!v.error.empty() || v.status != 200 || v.body.empty()) { error = "téléchargement de l'image impossible (HTTP " + std::to_string(v.status) + ")"; return false; }
    png.assign(v.body.begin(), v.body.end());
    return true;
}

}  // namespace sx::comfy
