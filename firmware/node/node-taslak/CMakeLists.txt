cmake_minimum_required(VERSION 3.16)

# Ortak katman burada. Yeni bir node projesi kopyalandiginda bu yol
# projenin konumuna gore guncellenmeli.
set(EXTRA_COMPONENT_DIRS ../common ../../../contract)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(node_taslak)
