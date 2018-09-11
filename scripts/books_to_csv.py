#!/usr/bin/env python3

import csv
import glob
import os

id = 0

def breakup_book(book):
    broken_up_book = list()
    index = 0
    text_block_size = 1048576
    slice = 0
    slices = int(len(book[2])/1048576) + 1

    while (slice < slices):
        broken_up_book.append((book[0], book[1], book[2][index:index+text_block_size], slice))
        index += text_block_size
        slice += 1

    return broken_up_book

def write_to_csv(csv_tuples, filename):
    with open(filename, 'w') as file:
        writer = csv.writer(file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
        writer.writerows(csv_tuples)

def open_book(path):
    global id
    with open(path, 'r') as file:
        title = ' '.join(os.path.basename(file.name).split('.')[0].split('_'))
        book_id = id
        id += 1
        text = file.read()

    return (book_id, title, text)


if __name__ == '__main__':
    books_dir = '/' + os.path.join(*(os.path.abspath(__file__).split(os.path.sep)[:-2] + ['tests/data/public_domain_books/*.txt']))
    book_paths = glob.glob(books_dir)
    broken_up_books = list()

    for book_path in book_paths:
        broken_up_books.extend(breakup_book(open_book(book_path)))

    write_to_csv(broken_up_books, 'public_domain_books.csv')
