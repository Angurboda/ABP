#include "math_util.h"

double myPower(double base, int exp) {
	double result = 0;
	while(exp>0) {
		result += base;
		exp--;
	}
	return result;
}

double mySum(int i, double expression) {

}
double myLn(double x);
double myLog10(double x);
