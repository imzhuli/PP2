alias genpb='python3 _script/gen_pb.py'
alias rebuild='python3 _script/build_service.py -x ../CoreX -d ../PP2DB'
alias rebuild_release='CMAKE_BUILD_TYPE=Release python3 _script/build_service.py -x ../CoreX'
alias qb='python3 _script/clean_install.py; cmake --build _build/cpp -- install'
alias rb='rebuild -j 24'


