if(MSVC)
  # https://github.com/boostorg/build/issues/299
  hunter_config(Boost VERSION 1.66.0-p0)
elseif(MINGW)
  # https://github.com/boostorg/build/issues/301
  hunter_config(Boost VERSION 1.64.0)
else()
  hunter_config(Boost VERSION 1.67.0-p1)
endif()

hunter_config(OpenSSL VERSION 1.1.0h)