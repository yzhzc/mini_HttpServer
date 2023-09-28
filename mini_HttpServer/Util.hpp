#pragma once

#include <iostream>
#include <string>
#include <vector>

// 工具类
class Util
{
public:
    // 读取一行Http请求
    static int ReadLine(int sock, std::string &out)
    {
        char ch = 'X';
        while (ch != '\n')
        {
            // 每次只读取一个字符
            ssize_t n = recv(sock, &ch, 1, 0);
            if (n > 0)
            {
                // 处理行请求末尾结束符，可能组合："\n", "\r", "\r\n"
                if (ch == '\r')
                {
                    // MSG_PEEK:窥探，读取数据但不取走
                    recv(sock, &ch, 1, MSG_PEEK);
                    if (ch == '\n')
                    {
                        // 确定"\r"后面为"\n", 就可以读走"\n"
                        recv(sock, &ch, 1, 0);
                    }
                    else
                    {
                        // "\r"后面不是"\n", 只用了“\r"作为末尾结束符,
                        // 我们给它添加一个"\n"形成格式统一
                        ch = '\n';
                    }
                }
                out.push_back(ch);
            }
            else if (n == 0)
            {
                // 对方关闭
                return 0;
            }
            else
            {
                // 读取出错
                return -1;
            }
        }

        return out.size();
    }

    // 提取分隔符前后的key和value
    static bool CurString(const std::string &target, std::string &sub1_out, std::string &sub2_out, std::string sep)
    {
        size_t pos = target.find(sep);  //查找分隔符位置
        if(pos != std::string::npos)
        {
            sub1_out = target.substr(0, pos);    // 截取分隔符前面的key
            sub2_out = target.substr(pos + sep.size());    // 截取分隔符后方的内容
            return true;
        }

        return false;
    }

    // 字符串转换为大写
    static void ToUpperCase(std::string &str)
    {
        std::transform(str.begin(), str.end(), str.begin(), toupper);
    }
};