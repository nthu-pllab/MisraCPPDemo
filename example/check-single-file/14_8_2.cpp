
void f(short s);

template <typename T>
void f(T s);

int b(short s) {
  f(s);
  f(s + 1);
  f<>(s);
  f<>(s + 1);
}
