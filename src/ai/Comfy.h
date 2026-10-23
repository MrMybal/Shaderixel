#pragma once
// ComfyUI client: runs a workflow in API format ("Save (API format)" in ComfyUI)
// on a local server and returns the first output image. Without a workflow a
// built-in text-to-image (or image-to-image) graph is used.
//
// Substitutions in a user workflow:
//   CLIPTextEncode text: "{prompt}" / "{negative}" placeholders, otherwise the
//     prompt goes to text encoders whose title does not contain "neg" (the
//     negative prompt, if given, to those that do);
//   KSampler seed / KSamplerAdvanced and RandomNoise noise_seed: random;
//   EmptyLatentImage (and SD3/Hunyuan variants) width / height;
//   LoadImage image: the uploaded input image (when one is sent).

#include "core/Json.h"

#include <functional>

namespace sx::comfy {

struct Job {
    std::string endpoint = "http://127.0.0.1:8188";
    std::string workflow;      // API-format JSON text, empty = built-in graph
    std::string checkpoint = "sd_xl_base_1.0.safetensors";  // built-in graph only
    std::string prompt, negative;
    int width = 1024, height = 1024;
    int steps = 25;            // built-in graph only
    float denoise = 0.6f;      // built-in image-to-image only
    std::vector<u8> inputPng;  // optional input image (LoadImage / image-to-image)
};

// Built-in graph (text-to-image, or image-to-image when `img2img`).
Json BuiltinWorkflow(const Job& job, bool img2img);
// Applies the substitutions above; returns a short summary of what was set.
std::string ApplyJob(Json& workflow, const Job& job, const std::string& uploadedImage, u64 seed);

// Blocking (call from a worker thread). `progress` receives status lines.
bool Run(const Job& job, std::vector<u8>& png, std::string& error, const std::function<void(const std::string&)>& progress = {});
// Every running job stops waiting (the application closes).
void CancelAll();

}  // namespace sx::comfy
