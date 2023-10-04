/* 
    Parallel Web Scraper 
    By Drew Hubble
    
    Instructions:
    1) Create a folder in your root directory titled "wiki"
    2) Compile the program with the following:
        mpic++ -std=c++11 -o wiki_crawler.out wiki_crawler.cpp -lcurl
    2) Use the following in your terminal to run the program, entering some number for <number_of_processes>:
        time mpirun -np <number_of_processes> --oversubscribe ./wiki_crawler.out
*/

#include <iostream>
#include <fstream>
#include <string>
#include <curl/curl.h>
#include <mpi.h>


// function used to handle the data returned by the HTTP request
static size_t WriteCallback(char *ptr, size_t size, size_t num_mem_bytes, void *user_data) {
    ((std::string*)user_data)->append(ptr, size * num_mem_bytes);
    return size * num_mem_bytes;
}

// function to return the HTML script of the web page directed to by the url
std::string ScrapeURL(std::string& url) {
    CURL *curl;
    std::string html_content;
    long response_code;

    curl = curl_easy_init();                                                        // intitialize curl
    if (!curl) {
        std::cout << "Error: Unable to initialize curl\n";
        return "";
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());                               // set the URL for curl to send a request to
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);                   // set the callback function to the callback function created above
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &html_content);                       // write the data from the request into html_content variable

    CURLcode response = curl_easy_perform(curl);                                    // perform the HTML request
    if (response != CURLE_OK) {                                                     // if the request was not successful
        std::cout << "Error: Request not successful\n";
        return "";                                                                  // return an empty string
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);                // get the response code
    curl_easy_cleanup(curl); // curl cleanup
    if (response_code != 200 && response_code != 301 && response_code != 302) {     // if the site does not permit web scraping
        std::cout << "Error Response: Site does not permit web scraping\n";
        return "";                                                                  // return an empty string
    }

    return html_content;                                                            // return the HTML content
}

// tag struct for the parser
struct tag {
    size_t start_o;     // start (<) of the open tag (<tag_name>)
    size_t start;       // end (>) of the open tag (<tag_name>)
    size_t end;         // start (<) of the close tag (</tag_name>)
    size_t length;      // length of the string (between start and end)
    size_t next;        // start (<) of the next open tag from the end of the current tag
};

// function to return a 'tag' struct of the next instance of <str> from a given index
tag StringParser(std::string script, size_t index, std::string str) {
    std::string open_tag = "<" + str;                   // string for the open tag 
    std::string close_tag = "</" + str + ">";           // string for the close tag
    size_t start_o = script.find(open_tag, index);      // find the index of the start of the open tag
    if (start_o == std::string::npos)                   // if there are no tags left, then
        return {};                                      // return an empty struct
    size_t start = script.find(">", start_o) + 1;       // find the index for the end of the open tag
    size_t end = script.find(close_tag, start);         // find the index of the start of the close tag
    if (end == std::string::npos)                       // if there is no end tag, then
        end = script.find("<", start);                  // set end to the index of the next instance of "<"
    size_t length = end - start;                        // set the length of the string
    size_t next = script.find(open_tag, end);           // find the index of the start position for the next tag
    return {start_o, start, end, length, next};         // return the tag struct
}

// function to write tag contents to a file
void WriteFile(std::string& script, tag _tag_, std::string filename) {
    if (_tag_.length == 0)                                              // if the tag has no contents, return
        return;
    char buff[_tag_.length + 1];                                        // initialize a buffer to add tag contents to
    size_t buff_pos = 0;                                                // initialize the buffer position
    size_t start_write = _tag_.start;                                   // initialize the start write position to the tag start
    size_t write_length;                                                // initialize the write length
    size_t nbs_start = script.find("&", start_write);                   // find the next '&'
    size_t nbs_end = script.find(";", nbs_start) + 1;                   // find the next ';'
    while (nbs_start < _tag_.end && nbs_end < _tag_.end) {              // while there are '&' and ';' inside of the tag, then
        write_length = nbs_start - start_write;                         // write until '&'
        if (write_length > 0)                                           // if there is anything to write, then
            script.copy(buff + buff_pos, write_length, start_write);    // write the contents
        buff_pos += (nbs_end - start_write);                            // move the buffer position
        start_write = nbs_end;                                          // move the start write position to ';'
        nbs_start = script.find("&", nbs_end);                          // find the next '&'
        nbs_end = script.find(";", nbs_start);                          // find the next ';'
    }
    write_length = _tag_.end - start_write;                             // set the write length
    script.copy(buff + buff_pos, write_length, start_write);            // copy tag contents to the buffer
    buff[_tag_.length] = '\0';                                          // add string terminator

    std::ofstream outfile(filename, std::ios::app);                     // open the file passed as parameter
    outfile << buff;                                                    // write buffer contents to the file
    outfile.close();                                                    // close the file
    std::fill(buff, buff + _tag_.length + 1, '\0');                     // empty the buffer
}

// function to parse html and only write paragraph content
void ParseHTML(std::string script, std::string filename) {
    size_t script_pos = 0;                                              // set the initial script position
    tag paragraph, next_tag, span, sup, i;                              // initialize the tags
    do {
        paragraph = StringParser(script, script_pos, "p");              // find the next paragraph
        script_pos = paragraph.start_o;                                 // move script position to the start of open paragraph tag
        do {
            next_tag = StringParser(script, script_pos, "");            // find the position of the next tag (any)
            span = StringParser(script, script_pos, "span");            // find the position of the next <span> tag
            sup = StringParser(script, script_pos, "sup");              // find the position of the next <sup> tag
            script_pos = next_tag.start;                                // set script position to the end of next open tag
            if (script_pos != span.start && script_pos != sup.start) {  // if the next tag is not <span> or <sup>
                WriteFile(script, next_tag, filename);                  // write the contents to a file
                script_pos = next_tag.end;                              // move script position to the end of the tag
            }
            else {                                                      // if the next tag is <span> or <sup>
                i = StringParser(script, script_pos, "i");              // find the next <i> tag
                if (i.start != 0 && i.start < next_tag.end)             // if the <i> tag is inside of <span> or <sup>
                    WriteFile(script, i, filename);                     // write the contents of the i tag
                if (script_pos == span.start)                           // if the tag was a <span> tag, then
                    script_pos = span.end;                              // move the position to the end of <span> tag
                if (script_pos == sup.start)                            // if the tag was a <sup> tag, then
                    script_pos = sup.end;                               // move the position to the end of <sup> tag
            }
        } while (next_tag.start < paragraph.end);                       // loop while the next tag is inside of a paragraph
    } while (paragraph.next != 0);                                      // loop for all paragraphs
}

// struct for the return values of the crawler
struct crawler {
    std::string url;    // the url returned by the crawler
    size_t pos;         // the position at the end of the url
};

// function to crawl html script from a given position and find the next valid url
crawler WikiCrawler(std::string script, size_t crawler_pos) {
    size_t start_table = script.find("<table", crawler_pos);                                            // find the next <table> tag
    size_t end_table = script.find("</table>", crawler_pos);                                            // find the next </table> tag
    crawler_pos = script.find("href", crawler_pos);                                                     // move to the next href
    if (start_table < crawler_pos && crawler_pos < end_table)                                           // if <href> is inside of a <table> tag
        crawler_pos = script.find("href", end_table);                                                   // find the next <href> tag outside of the table
    size_t start_page_name = script.find("\"", crawler_pos) + 1;                                        // find the first qoute(")
    size_t end_page_name = script.find("\"", start_page_name);                                          // find the next qoute(")
    crawler_pos = end_page_name;                                                                        // move crawler position to the end of the url
    std::string page_name = script.substr(start_page_name + 1, end_page_name - start_page_name - 1);    // get the page name
    if (script.find("File:", start_page_name) < end_page_name)                                          // if the url is for a file, then
        return {"", crawler_pos};                                                                       // return empty url
    /* ----------------- valid page name ----------------- */
    if (script.find("/wiki", start_page_name) == start_page_name)                                       // if url is for 'wiki' page, then
        return {page_name, crawler_pos};                                                                // return the url
    /*-----------------------------------------------------*/
    else                                                                                                // for any other url, 
        return {"", crawler_pos};                                                                       // return empty url
}

int main(int argc, char** argv) {
    int procid, numprocs;                                                                           // initialize number of processes and process ID variable
    MPI_Init(&argc, &argv);                                                                         // initialize MPI
    MPI_Comm_rank(MPI_COMM_WORLD, &procid);                                                         // get the process ID
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);                                                       // get the number of processes

    if (numprocs < 2) {                                                                             // if less than two processes, then
        std::cerr << "Error: Program requires at least two processes.\n";                           // finalize processes and end program
        MPI_Finalize();
        return 1;
    }

    /*------------------------------only process 0-------------------------------*/
    if (procid == 0) {                                                                              // only first process proceeds
        int num_links_to_scrape;
        do {
            std::cout << "Enter a number of links to scrape: ";                                     // ask for the number of links to scrape
            std::cin >> num_links_to_scrape;
        } while (num_links_to_scrape != (int)num_links_to_scrape);                                  // while the value provided is not an integer, ask again

        std::string seed_url = "https://en.wikipedia.org/wiki/Wikipedia";                           // the seed URL to get other URL's from
        crawler _crawler;
        std::string script_to_crawl = ScrapeURL(seed_url);                                          // get the html contents of seed URL
        tag paragraph = StringParser(script_to_crawl, 0, "p");                                      // find the first paragraph
        size_t crawler_pos = paragraph.start;                                                       // set the position to the start of first paragraph
        for (size_t i = 0; i < num_links_to_scrape; i++) {                                          // loop for the number of links to scrape
            do {
                _crawler = WikiCrawler(script_to_crawl, crawler_pos);                               // get a crawler struct from the crawler position
                crawler_pos = _crawler.pos;                                                         // move the crawler position to the position of new struct
            } while (_crawler.url.length() == 0);                                                   // do this while the URL returned is empty
            std::string URL_NAME = _crawler.url;                                                    // get the URL name
            int URL_SIZE = URL_NAME.size() + 1;                                                     // get the URL size + 1
            MPI_Status STATUS;                                                                      // set status of process
            int PROC_TO_SEND_TO;                                                                    // initialize variable to hold process ID of process to send to
            MPI_Recv(&PROC_TO_SEND_TO, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &STATUS);     // receive the process ID of the first available process
            MPI_Send(URL_NAME.c_str(), URL_SIZE, MPI_CHAR, PROC_TO_SEND_TO, 0, MPI_COMM_WORLD);     // send the process the URL name
        }
        for (size_t i = 1; i < numprocs; i++) {                                                     // send each process a termination message
            char terminate = '\0';
            MPI_Send(&terminate, 1, MPI_CHAR, i, 0, MPI_COMM_WORLD);
        }
    }

    /*----------------------------all other processes-----------------------------*/
    else {
        while(true) {
            MPI_Send(&procid, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);                                // send process 0 the process ID
            char URL_NAME[1024];                                                                // initialize a buffer to hold the URL
            MPI_Recv(URL_NAME, 1024, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);        // receive the URL from process 0
            if (URL_NAME[0] == '\0') {                                                          // if the url is the termination message, then exit loop
                break;
            }
            std::string url_name = std::string(URL_NAME);                                       // convert URL name to string

            std::string url = "https://en.wikipedia.org/" + url_name;                           // concatenate URL name with wikipedia start for full URL
            std::string script_to_parse = ScrapeURL(url);                                       // scrape the URL
            std::string filename = url_name + ".txt";                                           // create the filename
            std::ofstream outfile;                                                              // initialize out-filestream
            outfile.open(filename, std::ofstream::out | std::ofstream::trunc);                  // open the file
            if (outfile.is_open()) {                                                            // if the file is open, then
                outfile.close();                                                                // close the file
                ParseHTML(script_to_parse, filename);                                           // parse the html script
                std::cout << "Process " << procid << " Wrote File:\n      " << filename << "\n";
            }
        }
    }

    std::cout << "Process " << procid << " Finished\n";

    MPI_Finalize();                                                                             // finalize MPI process
    
    return 0;
}