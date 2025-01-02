extends Control
## Test scene to tokenize and compare binary and source code sizes.

# Enum with state.
enum State {
	ON,
	OFF
}

# Constant.
const TWEEN_TIME = 1

# Onready variable.
@onready var grid := $Panel/Grid as GridContainer
@onready var display := $Box/Grid as GridContainer
# Private variable.
var _layout := {}
var _states := {}

# Internal method.
func _ready() -> void:
	for square in grid.get_children():
		(square as Control).mouse_entered.connect(_enter_square.bind(square))
		(square as Control).mouse_exited.connect(_leave_square.bind(square))
		_layout[square.name] = null
		_states[square.name] = State.OFF
	$Box/ButtonReset.pressed.connect(reset_all)
	$Box/ButtonSetall.pressed.connect(set_all)
	$Box/ButtonClose.pressed.connect(close)

# Internal method.
func _process(_delta: float) -> void:
	for square in display.get_children():
		square.modulate = Color.GREEN if _states[square.name] == State.ON else Color.RED

# Private method.
func _enter_square(square : CanvasItem) -> void:
	var tween := _layout.get(square.name) as Tween
	if tween and tween.is_running():
		tween.kill()
	square.modulate = Color.WHITE
	_states[square.name] = State.ON

# Private method.
func _leave_square(square : CanvasItem) -> void:
	var tween := _layout.get(square.name) as Tween
	if tween and tween.is_running():
		tween.kill()
	tween = square.create_tween().set_ease(Tween.EASE_IN).set_trans(Tween.TRANS_EXPO)
	tween.tween_property(square, ^"modulate", Color.TRANSPARENT, TWEEN_TIME)
	tween.tween_callback(func(): _states[square.name] = State.OFF)
	_layout[square.name] = tween

# Public method.
func reset_all() -> void:
	for square in grid.get_children():
		var tween := _layout.get(square.name) as Tween
		if tween and tween.is_running():
			tween.kill()
		square.modulate = Color.TRANSPARENT
		_states[square.name] = State.OFF

# Public method.
func set_all() -> void:
	for square in grid.get_children():
		_enter_square(square)
		_leave_square(square)

# Public method.
func close() -> void:
	if get_tree().root == get_parent():
		get_tree().quit()
	else:
		queue_free()
