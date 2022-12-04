extends "out_of_order_inheritance_b.notest.gd"

var test_val = test_val_base % test_fn()

class AA extends BA:
	const test_val_aa = test_val_a

class AB:
	const test_val_base = test_val_ba
	
	func test_fn():
		return "godot"

const test_val_a = "waiting for %s"
