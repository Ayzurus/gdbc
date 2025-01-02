# GD Bytecode Compiler

This is a port of the [Godot engine's GDscript tokenizer](https://github.com/godotengine/godot/tree/master/modules/gdscript) into GDExtension format to be freely called upon from GDscript itself.

The core idea is to allow the binary tokenization from Godot 4.3 to be called from any point of the editor or even in-game, allowing to compile text scripts in text format into binary tokens at any point.

Since the GDExtension API is not an exact replica of the engine's code, there are a few adaptations for the tokenizer to work with the equivalent source available on the GDExtension side.

## Usage

The addon will add an object called `BytecodeCompiler` which, when instantiated, will allow to compile a `GDScript` object or its source code directly into a `PackedByteArray` of bytecode.

It is also possible to compress the binary tokens with zstd, just like in the exporting options, using the `compression` flag argument.

- `BytecodeCompiler.UNCOMPRESSED` will have the same result as the export option `Binary tokens (faster loading)`.
- `BytecodeCompiler.COMPRESSED` will have the same result as the export option `Compressed binary tokens (smaller files)`.

By default, the bytecode compilation will be uncompressed.

Compilling from any GDScript or source code:

```gdscript
# Instantiate the compiler.
var compiler := BytecodeCompiler.new()

# Compile the current script object.
var bytes := compiler.compile_from_script(get_script())

# Compile the current script object's source code.
var script := get_script()
bytes = compiler.compile_from_string(script.source_code)

# In case we wish to compress later, instead of during compression.
bytes = compiler.compress(bytes)
```

## Decompilling

This addon is only aimed at the compilation aspect assuming the scripts will always be used inside the engine, which means that decompilling is not necessary, since it is done by the engine when the file is `load()` or `preload()`, whence most decode/decompilling code was not ported.

## Demo project

The demo project includes a pre-exported script in both compressed and uncompressed binary token formats.

When run the project showcases basic statistics regarding the expected binary tokens of the test script, and when the compillation is called, it updates with the statistics of the actual result produced by the execution of the `BytecodeCompiler`.

There are also options to replace the scripts in the scene and run both binary token versions to validate manually if they perform correctly.

The test scene can be replaced with anything and works just as a placeholder to validate the results of the compiller.

In case of replacing the test scene, don't forget to export the original in the engine as binary tokens and replace both `test/expected_uncompressed.gdc` and `test/expected_compressed.gdc` in order to have a valid comparison with the new scene.
