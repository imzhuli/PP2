#include <pp_common/lang/dispatcher.hpp>

void TF1(int a) {
    cout << "1.a=" << a << endl;
    a++;
    cout << a << endl;
}

int main(int argc, char **) {

    auto C = xDispatcher<int>();

    auto Index1 = C.Append(TF1);

    cout << "I1=" << Index1 << endl;

    return 0;
}
