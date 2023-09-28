#include <iostream>
#include <cstdlib>
#include <unistd.h>

// 获取请求的参数
bool GetQueryString(std::string &query_string)
{
    bool result = false;
    std::string method = getenv("METHOD"); // 获取方法

    if (method == "GET")
    {
        query_string = getenv("QUERY_STRING"); // 获取GET方法的参数
        result = true;
    }

    if (method == "POST")
    {
        int content_lenght = atoi(getenv("CONTENT_LENGHT")); // 获取POST方法主体正文的大小
        std::cout <<"CGI : Method = POST,Content-Lenght = " << content_lenght << std::endl;

        char c = 0;
        while (content_lenght)
        {
            read(0, &c, 1);
            query_string.push_back(c);
            content_lenght--;
        }
        result = true;
    }

    return result;
}

bool CurString(const std::string &in, const std::string &sep, std::string &out1, std::string &out2)
{
    size_t pos = in.find(sep); // 查找分隔符位置
    if (pos != std::string::npos)
    {
        out1 = in.substr(0, pos);           // 截取分隔符前面的key
        out2 = in.substr(pos + sep.size()); // 截取分隔符后方的内容
        return true;
    }

    return false;
}

int main()
{
    // 获取请求的参数
    std::string query_string;
    GetQueryString(query_string);

    // 将参数从字符串中截取出来
    std::string str1;
    std::string str2;
    CurString(query_string, "&", str1, str2);

    std::string name1;
    std::string value1;
    CurString(str1, "=", name1, value1);

    std::string name2;
    std::string value2;
    CurString(str2, "=", name2, value2);

    std::cout << name1 << ":" << value1 << std::endl;
    std::cout << name2 << ":" << value2 << std::endl;

    std::cerr << name1 << ":" << value1 << std::endl;
    std::cerr << name2 << ":" << value2 << std::endl;

    int x = atoi(value1.c_str());
    int y = atoi(value2.c_str());

    std::cout << "<html>";
    std::cout << "<head><meta charset=\"UTF-8\"></head>";
    std::cout << "<body>";
    std::cout << "<h3>" << value1 << "+" << value2 << "=" << x + y << "</h3>";
    std::cout << "<h3>" << value1 << "-" << value2 << "=" << x - y << "</h3>";
    std::cout << "<h3>" << value1 << "*" << value2 << "=" << x * y << "</h3>";
    std::cout << "<h3>" << value1 << "/" << value2 << "=" << x / y << "</h3>";
    std::cout << "</body>";
    std::cout << "</html>";

    return 0;
}