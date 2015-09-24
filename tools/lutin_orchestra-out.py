#!/usr/bin/python
import lutin.module as module
import lutin.tools as tools
import lutin.debug as debug

def get_desc():
	return "'out' tool for orchestra"


def create(target):
	my_module = module.Module(__file__, 'orchestra-out', 'BINARY')
	
	my_module.add_src_file([
		'orchestra-out.cpp'
		])
	my_module.add_module_depend(['audio-orchestra', 'test-debug'])
	return my_module


