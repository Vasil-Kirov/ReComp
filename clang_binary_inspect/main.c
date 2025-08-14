
__attribute__((no_sanitize("address")))
int foo(int a);

int main()
{
	int x = foo(1);
	return x;
}

