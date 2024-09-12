#include <iostream>  
using namespace std;

// Задача 1
void findChisle() {
    int x;
    cout << "Введите целое число: ";
    cin >> x;

    cout << "Числа, на которые " << x << " делится без остатка:" << endl;

    for (int y = 1; y <= x; ++y) {
        if (x % y == 0) {
            cout << y << endl;
        }
    }
}

// Задача 2
void Ugadaika() {
    cout << "Загадайте число от 0 до 1000 а я попытаюсь его угадать" << endl;

    int low = 0;
    int high = 1000;
    int popitka = 0;
    char luckeokay;

    while (true) {
        int guess = (low + high) / 2;
        popitka++;
        cout << "Мне кажется, что это " << guess << " Правильно? (введите 'y' для да 'b' для больше 'm' для меньше): ";
        cin >> luckeokay;

        if (luckeokay == 'y') {
            cout << "Ура! Я угадал число " << guess << " за " << popitka << " попыток." << endl;
            break;
        }
        else if (luckeokay == 'b') {
            low = guess + 1;
        }
        else if (luckeokay == 'm') {
            high = guess - 1;
        }
        else {
            cout << "Пожалуйста, отвечайте 'y', 'b' или 'm'." << endl;
        }
    }
}

// Задача 3 
void calculate() {
    double x, y;
    cout << "Введите размер стипендии (грн): ";
    cin >> x;
    cout << "Введите размер расходов (грн): ";
    cin >> y;

    double totalNeeded = 0;
    int months = 10;

    for (int month = 0; month < months; ++month) {
        totalNeeded += y;
        y *= 1.03;
    }

    totalNeeded -= x * months;

    if (totalNeeded > 0) {
        cout << "Вам нужно попросить у родителей " << totalNeeded << " гривен." << endl;
    }
    else {
        cout << "Стипендия покрывает все расходы." << endl;
    }
}


// Задача 4 
void elka() {
    int levels, height;
    cout << "Введите количество ярусов елочки: ";
    cin >> levels;
    cout << "Введите высоту каждого яруса: ";
    cin >> height;

    for (int level = 0; level < levels; ++level) {
        for (int h = 0; h < height; ++h) {
            for (int spaces = 0; spaces < (levels - level - 1) + (height - h - 1); ++spaces)
                cout << ' ';
            for (int stars = 0; stars < (1 + 2 * (level + h)); ++stars)
                cout << '*';
            cout << endl;
        }
    }
}

// Основная программа  
int main() {
    findChisle();
    Ugadaika();
    calculate();
    elka();
    return 0;
}