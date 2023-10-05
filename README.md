# wiki-crawler

# Description

* This program is a parallelized web crawler and scraper, specifically designed for wikipedia pages.
* The web crawler is contained in one process and it begins at the wikipedia page for 'wikipedia' and from there it searches the page for urls to other wikipedia pages and sends them to the other processes (the scraper processes). When the crawler process reaches the end of a page it moves to the page that is the first link and it crawls that page for more urls. When it sees a url that has already been scraped it skips it.
* The number of scraper processes is determined by the user and it is limited by the number of cores in your machine, but more processes can be simulated using "--oversubscribe".
* The scraper processes receive urls from the crawler process in a FIFO queue. When a scraper process receives a url, it scrapes the paragraph content of the url and writes that data to a text file with the name of the wikipedia page inside of a 'wiki_files' folder. The scraper process then goes back to the queue to receive another url.

# Dependencies

* This program requires you to install the open mpi and curl libraries.

# How to run the program
* The program can be compiled on mac using the following command:
```
mpic++ -std=c++11 -o wiki_crawler.out wiki_crawler.cpp -lcurl
```
* The program can be run on mac using the following command:
```
time mpirun -np <number_of_processes> --oversubscribe ./wiki_crawler.out
```

# Author

Drew Hubble

