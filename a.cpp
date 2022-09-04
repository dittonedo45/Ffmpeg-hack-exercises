#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <mutex>

using namespace std;

int func (int mm)
{
	for (int i=0; i<mm; i++)
	{
		throw i;
		cout << i << endl;
	}
	return 0;
}
struct thd_t : public thread
{
	thd_t (int m) : thread (func, m)
	{
	}
};

int main ( int Argsc, char *(Args[]) ) {
	try {
		thd_t thd (100);
		thd.join ();
	} catch (exception &exp)
	{
	} catch (int &i)
	{
		cout << i << endl;
	}
	return 0;
}

