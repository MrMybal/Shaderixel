---
id: blending
title: Blending and Blend If
description: A blend combines background and foreground.
section: Handbook
order: 7
parent: layers
revision: 7372ef77f531eeee
---

A blend combines background and foreground. Opacity scales the foreground contribution. Blend modes change RGB interaction; masks and alpha determine coverage.

## Modes and shader blends {#modes-and-shader-blends}

Choose the active layer's mode in Properties or Blend. Normal composites using coverage. Multiply, Screen and other modes create different color interactions. Shader blend effects expose background, layer and mask inputs; swapping foreground and background can change the result.

An opaque generator can hide all layers below it in Normal mode. Use transparency, opacity or the appropriate blend for an overlay.

## Blend If {#blend-if}

Blend If conditions visibility on the layer's own tones or the underlying image. Choose Gray or a color channel and adjust dark/light thresholds.

Each bar has four ordered 0-255 values: dark start, dark end, light start and light end. Split endpoints create gradual transitions. Alt-drag splits handles; the four fields below each bar allow exact numeric entry. Changing a threshold keeps the set ordered.

Check both bars when an apparently opaque layer has missing regions.

## Color and alpha {#color-and-alpha}

Shaderixel uses linear colors. View > Blend in sRGB changes the document blending choice and can visibly change midtones.

Premultiplied sampling inside shaders avoids dark fringes at filtered transparent edges. It is separate from selecting an export format that supports alpha.

[Color, precision and data maps](doc:color)
[Masks and selections](doc:masks)
