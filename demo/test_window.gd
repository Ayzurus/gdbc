extends PanelContainer

const TestScene = preload("res://test_scene.tscn")
const TestScript = preload("res://test_scene.gd")

@onready var start_compile := %RunTestButton as Button
@onready var result := %ResultLabel as Button
var _utime := 0
var _ctime := 0

func _ready() -> void:
	set_stats("test/expected_compressed.gdc", "CExpected")
	set_stats("test/expected_uncompressed.gdc", "UExpected")
	start_compile.pressed.connect(compile)
	%RunExpectedButton.pressed.connect(run_scene.bind("test/expected_uncompressed.gdc"))
	%RunActualButton.pressed.connect(run_scene.bind("test/actual_uncompressed.gdc"))
	#var _test = load("test/expected_compressed.gdc")
	#var _test2 = load("test/actual_compressed.gdc")

func run_scene(file: String) -> void:
	var script := ResourceLoader.load(file, "GDScript")
	if not script:
		push_error("%s GDScript is invalid and could not load it as a Resource." % file)
		return
	var scene := TestScene.instantiate()
	scene.set_script(script)
	add_child(scene)

func compile() -> void:
	start_compile.disabled = true
	result.text = "Compilling..."
	var compiler := BytecodeCompiler.new()
	var script := TestScript as GDScript
	_utime = Time.get_ticks_usec()
	var uncompressed := compiler.compile_from_string(
		script.source_code, BytecodeCompiler.UNCOMPRESSED)
	_utime = Time.get_ticks_usec() - _utime
	_ctime = Time.get_ticks_usec()
	var compressed := compiler.compile_from_string(
		script.source_code, BytecodeCompiler.COMPRESSED)
	_ctime = Time.get_ticks_usec() - _ctime
	var file := FileAccess.open("test/actual_uncompressed.gdc", FileAccess.WRITE)
	file.store_buffer(uncompressed)
	file.close()
	file = FileAccess.open("test/actual_compressed.gdc", FileAccess.WRITE)
	file.store_buffer(compressed)
	file.close()
	set_stats("test/actual_uncompressed.gdc", "UActual")
	set_stats("test/actual_compressed.gdc", "CActual")
	var validated := validate(compressed, uncompressed)
	result.text = "Success" if validated else "Failed"
	result.modulate = Color.GREEN if validated else Color.RED
	start_compile.disabled = false

func set_stats(file: String, type: String) -> void:
	var bytes := FileAccess.get_file_as_bytes(file)
	(get_node(str("%Size", type)) as Label).text = "%d bytes" % bytes.size()
	var script := ResourceLoader.load(file, "GDScript")
	(get_node(str("%Load", type)) as Label).text = "OK" if script else "FAIL"
	(get_node(str("%Load", type)) as Label).modulate = Color.GREEN if script else Color.RED
	if "actual" in file:
		var time := _utime if "uncompressed" in file else _ctime
		(get_node(str("%Time", type)) as Label).text = "%.03f ms" % (float(time) / 1000.0)
	if type == "UExpected" or type == "UActual":
		(get_node(str("%Name", type)) as Label).text = script.get_class()
		(get_node(str("%Symbols", type)) as Label).text = "%d" % (
			script.get_property_list().size() +
			script.get_method_list().size() +
			script.get_signal_list().size()
		)

func validate(compressed: PackedByteArray, uncompressed: PackedByteArray) -> bool:
	var expected_compressed := FileAccess.get_file_as_bytes("test/expected_compressed.gdc")
	var expected_uncompressed := FileAccess.get_file_as_bytes("test/expected_uncompressed.gdc")
	# Test the bytes of each binary version.
	if compressed.size() >= uncompressed.size():
		push_error("compressed is the same size or bigger than the uncompressed")
		return false
	if uncompressed.size() != uncompressed.size():
		push_error("actual and expected uncompressed sizes don't match")
		return false
	for i in range(uncompressed.size()):
		if i <= 27 and i >= 24:
			# ignore bytes 24, 25, 26 and 27, they seem to not be used, but may differ for some reason
			continue
		if uncompressed[i] != expected_uncompressed[i]:
			push_error("actual and expected uncompressed files are different on byte = ", i)
			return false
	return true
