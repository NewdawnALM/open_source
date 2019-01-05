#include <stdio.h>
#include <iostream>
using namespace std;

template<typename T>
class IsClass
{
private:
	typedef char One;
	typedef struct { char a[2]; } Two;
	template<typename C> 
	static One test(int C::*) {}
	template<typename C>
	static Two test(...) {}
public:
	enum { Yes = sizeof(IsClass<T>::test<T>(0)) == 1 };
};

class CC
{
public:
	CC();
	~CC();
};

typedef struct { long lx; } mylong;
typedef long long LL;

int main(int argc, char const *argv[])
{
	cout << IsClass<CC>().Yes << "\n";
	cout << IsClass<int>().Yes << "\n";
	cout << IsClass<mylong>().Yes << "\n";
	cout << IsClass<LL>().Yes << "\n";
	return 0;
}