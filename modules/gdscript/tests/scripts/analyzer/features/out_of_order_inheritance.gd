func test():
	print("test")
	var t = preload("out_of_order_inheritance_a.notest.gd").new()
	print(t.test_val)
