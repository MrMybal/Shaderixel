---
id: image-graph
title: Main image graph
description: The main graph connects complete images, channels, values and execution actions.
section: Create / Graphs
order: 8
revision: fec167db155fe163
---

The main graph connects complete images, channels, values and execution actions. The visual effect graph instead defines the calculation inside one effect.

```image
{"id": "graphs", "src": "assets/manual/graphs_en.png", "alt": "Image graph versus the per-pixel graph inside one effect.", "controls": "minimal"}
```

## Build a visible result {#build-a-visible-result}

1. Start or convert to a Node-mode document.
2. Right-click empty Graph space and add an image source or Draw generator.
3. Add an effect and connect the source to its Image input.
4. Connect the final image to Output.
5. Select nodes to edit Properties and frame the view if nodes are offscreen.

A generator needs no image input. An image-processing effect normally does. A disconnected Output, empty surface or transparent source can produce a checkerboard.

## Types and parameters {#types-and-parameters}

Image pins carry images; channel pins carry scalar fields. Value pins carry numbers, colors, vectors or text. Execution pins launch actions. Types must be compatible.

Parameters as Pins exposes effect settings as inputs. Connected values control them; disconnected value inputs use defaults. Unconnected image behavior depends on the effect.

Split separates RGBA channels. Math and variable nodes drive controls. Multi-output functions require the intended output pin.

## Library and reuse {#library-and-reuse}

Right-click > Library inserts a .sxfn function. Properties exposes inputs and Variables; double-click inspects its graph. Subgraphs are live. Tools have execution buttons and can bake, run AI, iterate or export.

A tool may preview its input before execution; this does not mean AI or export has completed. Select nodes and use the context menu to create a reusable function, then Save in Library.

[Functions, execution and automation](doc:automation)
[Visual effect graph and shader editor](doc:effect-editor)

[Complete structural, value and execution node reference](doc:builtin-nodes)
