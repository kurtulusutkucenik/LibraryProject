cmake_minimum_required(VERSION 3.10)
project(KutuphaneSistemi C)

set(CMAKE_C_STANDARD 11)

include_directories(include)

add_executable(KutuphaneSistemi
    main.c
    common.c
    book.c
    member.c
    loan.c
    log.c
    config.c
    workspace.c
    sha256.c
    aes.c
    library.c
)