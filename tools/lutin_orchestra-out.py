#!/usr/bin/python
import lutin.module as module
import lutin.tools as tools
import lutin.debug as debug

def get_desc():
	return "'out' tool for orchestra"


def create(target):
	myModule = module.Module(__file__, 'orchestra-out', 'BINARY')
	
	myModule.add_src_file([
		'orchestra-out.cpp'
		])
	myModule.add_module_depend(['audio-orchestra', 'test-debug'])
	return myModule


