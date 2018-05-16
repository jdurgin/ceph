add_executable(ceph_test_objectcacher_stress
  object_cacher_stress.cc
  FakeWriteback.cc
  MemWriteback.cc
  )
target_link_libraries(ceph_test_objectcacher_stress
  osdc
  global
  ${EXTRALIBS}
  ${CMAKE_DL_LIBS}
  )
install(TARGETS ceph_test_objectcacher_stress
  DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(ceph_test_objecter
  test_objecter.cc
  )
target_link_libraries(ceph_test_objecter
  librados
  osdc
  global
  ${UNITTEST_LIBS}
  radostest
  )
install(TARGETS ceph_test_objecter
  DESTINATION ${CMAKE_INSTALL_BINDIR})
