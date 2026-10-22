from pathlib import Path
import csv
import html
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib import colors
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_CENTER
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, PageBreak, Table, TableStyle, Preformatted
from reportlab.lib.pagesizes import A4

root = Path(__file__).resolve().parents[2]
out = root / 'report' / 'lab3-report.pdf'
font_root = Path('/System/Library/Fonts/Supplemental')
pdfmetrics.registerFont(TTFont('ReportArial', str(font_root / 'Arial.ttf')))
pdfmetrics.registerFont(TTFont('ReportArialBold', str(font_root / 'Arial Bold.ttf')))
pdfmetrics.registerFontFamily('ReportArial', normal='ReportArial', bold='ReportArialBold', italic='ReportArial', boldItalic='ReportArialBold')
styles = getSampleStyleSheet()
styles.add(ParagraphStyle(name='BodyRU', fontName='ReportArial', fontSize=10, leading=14, spaceAfter=8))
styles.add(ParagraphStyle(name='TitleRU', fontName='ReportArialBold', fontSize=18, leading=24, alignment=TA_CENTER, spaceAfter=18))
styles.add(ParagraphStyle(name='HeadingRU', fontName='ReportArialBold', fontSize=14, leading=19, spaceAfter=12))
styles.add(ParagraphStyle(name='CenterRU', parent=styles['BodyRU'], alignment=TA_CENTER))
styles.add(ParagraphStyle(name='SmallRU', parent=styles['BodyRU'], fontSize=8, leading=11))
styles.add(ParagraphStyle(name='CodeRU', fontName='Courier', fontSize=8, leading=11, spaceAfter=12))
story=[]
def p(text, style='BodyRU'):
    story.append(Paragraph(text, styles[style]))
def heading(text):
    p(text, 'HeadingRU')
def code(text):
    story.append(Preformatted(text, styles['CodeRU']))
def page(): story.append(PageBreak())
def table(rows, widths):
    converted=[[Paragraph(html.escape(str(cell)), styles['SmallRU']) for cell in row] for row in rows]
    t=Table(converted,colWidths=widths,repeatRows=1,hAlign='LEFT')
    t.setStyle(TableStyle([
        ('BACKGROUND',(0,0),(-1,0),colors.HexColor('#E8EDF3')),
        ('GRID',(0,0),(-1,-1),0.4,colors.HexColor('#CAD2DC')),
        ('VALIGN',(0,0),(-1,-1),'TOP'),('LEFTPADDING',(0,0),(-1,-1),7),
        ('RIGHTPADDING',(0,0),(-1,-1),7),('TOPPADDING',(0,0),(-1,-1),5),
        ('BOTTOMPADDING',(0,0),(-1,-1),5)]))
    story.append(t)
    story.append(Spacer(1,10))

p('МОСКОВСКИЙ АВИАЦИОННЫЙ ИНСТИТУТ<br/>(НАЦИОНАЛЬНЫЙ ИССЛЕДОВАТЕЛЬСКИЙ УНИВЕРСИТЕТ)', 'CenterRU')
p('Институт № 8 «Компьютерные науки и прикладная математика»<br/>Кафедра 806 «Вычислительная математика и программирование»', 'CenterRU')
story.append(Spacer(1,100))
p('Лабораторная работа № 3', 'TitleRU')
p('по курсу «Операционные системы и архитектура компьютера»', 'CenterRU')
p('Управление потоками и синхронизация<br/>Вариант 9. Определитель матрицы', 'CenterRU')
story.append(Spacer(1,75))
p('Выполнил: А. А. Копылов<br/>Группа: М8О-204БВ-25<br/>Преподаватель: Н. А. Гапонов<br/>Дата: 22.10.2026')
story.append(Spacer(1,90))
p('Москва, 2026', 'CenterRU')
page()
heading('1. Задание и алгоритм')
p('Цель работы - освоить создание потоков ОС и синхронизацию доступа к общим данным. Требуется вычислить определитель матрицы на языке C, задавать максимальное число одновременно существующих потоков ключом запуска, показать потоки средствами ОС и исследовать ускорение и эффективность.')
p('Использовано рекурсивное разложение Лапласа по первой строке. Полное раскрытие даёт ту же знакопеременную сумму произведений по перестановкам, что и определение определителя. Метод Гаусса в программе на C не используется.')
code('D(A) = sum((-1)^j * A[0,j] * D(M[0,j]))\nD([a]) = a')
p('Вычеркивание первой строки и выбранного столбца реализовано функцией make_minor. Матрица хранится в одномерном массиве: элемент строки row и столбца col имеет индекс row*n + col. Размер входа: от 1 до 14. Элементы и результат имеют тип long double; для вещественных данных возможны ошибки округления. Верхняя граница размера ограничивает вход, но не гарантирует быстрое вычисление: сложность факториальная.')
p('Верхние d уровней раскрываются заранее в prepare_tasks. Каждая задача содержит собственную оставшуюся матрицу и накопленный множитель со знаком. По умолчанию d=2. Число задач равно n!/(n-k)!, где k=min(d,n-1). Например, для n=5 получается 20 задач. При d=0 остаётся одна задача.')
heading('2. Организация потоков')
p('Пул содержит min(лимит, число задач) потоков с учётом основного. Создаются только p-1 дополнительных потоков через pthread_create. Основной вызывает worker как обычную функцию, затем ожидает остальных через pthread_join. Поэтому -t 4 означает максимум четыре потока всего.')
p('Задачи находятся в готовом массиве. Общий индекс next защищён одним pthread_mutex_t. В критической секции поток проверяет наличие задачи и увеличивает индекс. Рекурсивное вычисление выполняется после освобождения мьютекса. Каждый Worker хранит отдельную сумму result. Основной складывает суммы после завершения всех работников. Матрицы задач после запуска не изменяются, временные миноры находятся в стеке выполняющего потока.')
code('lock(mutex)\nindex = next++  // only if a task is available\nunlock(mutex)\nlocal_sum += tasks[index].factor * determinant(minor)')
page()
heading('3. Сборка и проверка')
p('Основная проверка выполнена в Linux-контейнере: Linux 7.0.14-linuxkit, aarch64, GCC 13.3.0, glibc 2.39, восемь доступных логических CPU. Квота cpu.max: max 100000, то есть дополнительная квота cgroup не задана. Сборка CMake Release. Также выполнена сборка AppleClang 21 на macOS.')
code('cmake -S src -B /tmp/lab3-build -DCMAKE_BUILD_TYPE=Release\ncmake --build /tmp/lab3-build\nctest --test-dir /tmp/lab3-build --output-on-failure\n/tmp/lab3-build/determinant -t 4 examples/matrix3.txt')
p('Вход: размер n, затем n*n чисел по строкам. Файл необязателен: без него программа читает stdin. Пример matrix3.txt содержит матрицу [[6,1,1],[4,-2,5],[2,8,7]], ожидаемый определитель -306. Ключи: -t - лимит потоков; -d - глубина подготовки от 0 до 3; -w - пауза от 0 до 60 секунд для показа потоков. При замерах -w не используется.')
table([
['Проверка','Результат'],
['Точное сравнение','352 запуска: 22 матрицы, глубины 0..3, лимиты 1,2,4,8. Независимый эталон - сумма по перестановкам с рациональной арифметикой. Пройдены на macOS и Linux.'],
['Граничные случаи','Матрицы 1x1, нулевая и единичная; совпадающие строки; нулевой ведущий элемент; дробные значения; случайные матрицы до 7x7.'],
['Ошибки ввода','19 случаев: некорректные ключи, неполная матрица, лишние элементы, NaN/Inf, неверный размер, отсутствующий файл. Ошибки отклонены.'],
['Память','ASan+UBSan: полный точный набор на macOS; в Linux базовый набор с detect_leaks=1. Сообщений об ошибках нет.'],
['Потоки средствами ОС','Для -t 1,2,4,8 через ps и /proc зафиксированы соответственно 1,2,4,8 потоков.'],
], [130,365])
p('В базовом наборе CTest - 16 сочетаний глубины и числа потоков, случай 1x1 с лимитом 256 и пять ошибочных входов. При наличии Python3 CMake дополнительно включает полный точный набор. В исходном Linux-образе Python отсутствовал: полный набор запускался отдельно в уже имевшемся Python-контейнере на том же Linux-бинарнике.')
p('Ограничение проверки: ASan и UBSan не являются детекторами гонок данных. Отсутствие гонок обосновывается защитой next, неизменностью задач и раздельными результатами работников, читаемыми после join.', 'SmallRU')
page()
heading('4. Исследование ускорения и эффективности')
p('Использованы фиксированные матрицы размеров 5, 8, 10 и 11. Генератор Python Random(9000+n); элементы выбираются из {-3,-2,-1,1,2,3}. Входы сохранены в examples/benchmark*.txt. Каждый результат дополнительно проверен точным рациональным исключением в отдельном Python-драйвере. Это проверка результата, алгоритм программы на C остаётся рекурсивным.')
p('Для каждого размера выполнен один прогрев и пять измерений на 1,2,4,8 потоках. В таблице - медианы. Порядок конфигураций в каждом повторе детерминированно перемешан. Глубина подготовки всегда 2. Замеры выполнены без strace и без паузы -w. CLOCK_MONOTONIC охватывает подготовку задач, выделение памяти пула, создание потоков, вычисления, join и итоговое суммирование; чтение входа, запуск процесса, печать и освобождение памяти исключены.')
p('Ускорение S=T1/Tp. Эффективность E=S/p. Значение p учитывает основной поток. CSV с каждым измерением, минимумами и максимумами сохранены рядом с отчётом.')
rows=list(csv.DictReader((root/'report/benchmark-summary.csv').open()))
table([['n','Потоки','Медиана, с','S','E, %']]+[
[r['n'],r['threads'],f"{float(r['median_seconds']):.9f}",f"{float(r['speedup']):.3f}",f"{100*float(r['efficiency']):.1f}"] for r in rows
], [40,65,155,105,130])
p('Короткие запуски 5x5 измеряются десятками микросекунд и чувствительны к шуму планировщика. Для них значения ускорения не следует интерпретировать как универсальные характеристики системы.', 'SmallRU')
page()
heading('5. Объяснение результатов')
p('На матрице 5x5 создание потоков и синхронизация занимают больше времени, чем экономится на вычислениях. При увеличении размера факториальный объём рекурсии быстро растёт, а число созданий потоков остаётся постоянным. Поэтому на 11x11 ускорение на восьми потоках составило около 6,01, эффективность - 75,1%.')
p('Ускорение ограничивают последовательная подготовка задач, синхронизация выдачи, различное время завершения работников и особенности планирования CPU. При d=2 число задач равно n(n-1); при слишком большом лимите полезный параллелизм ограничен этим числом. Фактическое число потоков дополнительно ограничивается числом задач. Результаты относятся к указанной среде и не гарантируют такой же эффект на другом компьютере.')
heading('6. Демонстрация через strace и ps')
code('sh src/tools/capture-linux.sh /tmp/lab3-build/determinant \\\n    /tmp/lab3-evidence\ncat /tmp/lab3-evidence/ps-t4.txt\ngrep -E "clone|futex|exit" /tmp/lab3-evidence/strace-t4.txt')
p('Скрипт запускает программу под strace -f, добавляя двухсекундную паузу перед работой. Во время паузы снимаются ps -L и /proc/PID/status. Эта пауза нужна только для наблюдения; полученное время не используется в исследовании ускорения.')
code((root/'report/evidence/ps-t4.txt').read_text().strip())
p('Один PID объединяет четыре разных TID; NLWP=4. Для лимита 4 в трассировке записаны три успешных clone: дополнительные потоки создаются с флагами CLONE_VM, CLONE_FILES и CLONE_THREAD. Основной поток уже существует до запуска worker.')
p('В этой среде первая попытка clone3 вернула ENOSYS, после чего библиотека успешно использовала clone. Это совместимый запасной путь, а не ошибка вычисления. Вызов futex(...FUTEX_WAIT_BITSET...) наблюдается при ожидании завершения работника. Мьютекс без конкуренции обычно работает в пользовательском пространстве, поэтому каждому lock/unlock не обязан соответствовать системный вызов futex.')
p('В strace видны системные вызовы, а не названия библиотечных pthread_create/pthread_join. Параметр -f включает наблюдение за созданными потоками. exit завершает отдельный поток, exit_group - весь процесс. Файлы фактической трассировки сохранены в report/evidence/.')
page()
heading('7. Вывод')
p('Реализована программа на C с POSIX Threads для вычисления определителя рекурсивным разложением. Число потоков ограничивается ключом запуска и включает основной поток. Общая выдача задач защищена мьютексом, результаты работников разделены. Корректность подтверждена независимыми точными проверками; число потоков показано средствами Linux и трассировкой системных вызовов. Эксперимент подтвердил пользу многопоточности на достаточно крупных входах и накладные расходы на малых.')
heading('8. Структура файлов')
table([
['Файл','Назначение'],
['src/determinant.c','Программа, пул, рекурсия, разбор входа.'],
['src/CMakeLists.txt','Сборка и подключение тестов.'],
['src/tests/','Базовые проверки и точный независимый эталон.'],
['src/tools/capture-linux.sh','Демонстрация strace, ps и /proc.'],
['src/tools/benchmark.py','Воспроизводимые замеры и проверка результатов.'],
['src/tools/Dockerfile','Рецепт среды Linux; для текущей проверки использован существующий образ.'],
['examples/','Примеры и входы исследования.'],
['report/','PDF, исходные измерения и свидетельства демонстрации.'],
['DEFENSE.md','Памятка по функциям кода и вопросам на защите.'],
], [190,305])
heading('9. Источники')
for text in [
'Задание «Лабораторная работа 3», вариант 9, предоставленное для выполнения работы.',
'POSIX Threads: https://man7.org/linux/man-pages/man7/pthreads.7.html',
'clone(2): https://man7.org/linux/man-pages/man2/clone.2.html',
'futex(2): https://man7.org/linux/man-pages/man2/futex.2.html',
'strace(1): https://man7.org/linux/man-pages/man1/strace.1.html',
]: p(html.escape(text),'SmallRU')

def footer(canvas, doc):
    canvas.setFont('ReportArial',8)
    canvas.setFillColor(colors.HexColor('#606A78'))
    canvas.drawString(50,30,'Лабораторная работа № 3 | Вариант 9')
    canvas.drawRightString(A4[0]-50,30,str(doc.page))
SimpleDocTemplate(str(out), pagesize=A4, rightMargin=50,leftMargin=50,topMargin=45,bottomMargin=45,
                  title='Лабораторная работа 3. Вариант 9', author='А. А. Копылов').build(story,onFirstPage=footer,onLaterPages=footer)
print(out)
