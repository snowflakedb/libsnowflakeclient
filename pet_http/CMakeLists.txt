
set (CMAKE_CXX_STANDARD 11)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

add_executable(pet_http_runner
	PetHttpRunner.cpp
	)

target_link_libraries(pet_http_runner
	Threads::Threads
	)
