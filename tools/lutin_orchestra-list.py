#!/usr/bin/python
import lutin.module as module
import lutin.tools as tools
import lutin.debug as debug

def get_desc():
	return "'list' i/o tool for orchestra"


def create(target):
	my_module = module.Module(__file__, 'orchestra-list', 'BINARY')
	
	my_module.add_src_file([
		'orchestra-list.cpp'
		])
	my_module.add_module_depend(['audio-orchestra', 'test-debug'])
	return my_module


