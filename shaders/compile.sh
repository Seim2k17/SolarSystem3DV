#!/usr/bin/env sh

glslc shader.vert -o vert.spv
glslc shader.frag -o frag.spv
glslc gradient.comp -o compute.spv
