// #include <DipFantasy.h>
#include <opencv4/opencv2/opencv.hpp>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

//已知issue:目前霍夫变换检测出来的线是斜着的
//it is very slow!

using namespace cv;

static uchar *GetPoint1(Mat &input, int x, int y)
{
    int row_max = input.rows;
    int col_max = input.cols * input.channels();
    if (x < 0 || x >= row_max || y < 0 || y >= col_max)
    {
        return NULL;
    }
    uchar *p = input.ptr<uchar>(x);

    return p + y * input.channels();
}
//极坐标下的霍夫变换，霍夫变换中的一个点就对应直角坐标系中的一根线
class Hough_Fantasy
{
private:
    unsigned int *hough_mat_pointer;
    //排序?
    long long *sorted_pointer;

    Mat input_mat;

    //最大的长度
    int max_p;

    int sorted_pointer_position = 0;

    //阈值
    int threshold;

    //在霍夫变换的图像上的点加权
    void DrawInHoughMat(int x, int y);

    unsigned int *GetHoughPoint(int theta, int p);

public:
    Hough_Fantasy(Mat &input, int threshold);
    ~Hough_Fantasy();

    //开始霍夫变换
    void DoHoughTransition();

    //得到在霍夫坐标系中当前值最大的那个点的极坐标参数
    void GetOneHoughPoint(int &p_return, int &theta_return);

    //将极坐标的θ和半径转换为直角坐标的斜率和截距
    void ConvertPoleToXY(int p_input, int theta_input, int &k_return, int &b_return);

    //获得一条线的斜率和截距的参数
    void GetOneLine(int &k_return, int &b_return);

    //输入参数在原图画线
    static void PrintLineToImage(Mat &input, int p, int theta, int val);
};

void Hough_Fantasy::PrintLineToImage(Mat &input, int p, int theta, int val)
{
    int max_row = input.rows;
    double cost = cos(((double)theta / 180.0) * M_PI), sint = sin(((double)theta / 180.0) * M_PI);
    for (int i = 0; i < max_row; i++)
    {
        int y = (p - i * cost) / sint;

        for (int i2 = -3; i2 < 3; i2++)
        {
            for (int j2 = -3; j2 < 3; j2++)
            {
                uchar *point = GetPoint1(input, i + i2, y + j2);
                if (point != NULL)
                {
                    *point = val; //*(point+3)可以单独改红色的坐标,因为GetPoint已经乘了Channel
                }
            }
        }
    }
}

unsigned int *ugly = NULL;

int compare_func(const void *a, const void *b)
{
    return (int)(*(ugly + *(long long *)b) - *(ugly + *(long long *)a));
}

void Hough_Fantasy::GetOneHoughPoint(int &p_return, int &theta_return)
{
    if (sorted_pointer_position == 0)
    {
        long long the_size = (this->max_p + 1) * 2 * (90 + 180 + 1);
        this->sorted_pointer = (long long *)calloc(the_size, sizeof(long long));
        for (long long i = 0; i < the_size; i++)
        {
            sorted_pointer[i] = i;
        }
        qsort(this->sorted_pointer, the_size, sizeof(long long), compare_func);
    }

    //第一个就是那个的下标

    theta_return = sorted_pointer[sorted_pointer_position] / ((2 * max_p + 1)) - 90;
    p_return = sorted_pointer[sorted_pointer_position] % ((2 * max_p + 1)) - this->max_p;
    this->sorted_pointer_position++;
    return;
    //p + max_p

    /*
    unsigned int max = 0;
    int rt_theta = -1, rt_p = -1;
    int pmin = -(this->max_p), pmax = this->max_p;
    for (int theta = -90; theta <= 180; theta++)
    {
        for (int p = pmin; p <= pmax; p++)
        {
            unsigned int *point = GetHoughPoint(theta, p);
            if (point != NULL)
            {
                if (*point > max)
                {
                    max = *point;
                    rt_theta = theta;
                    rt_p = p;
                }
            }
        }
    }

    p_return = rt_p;
    theta_return = rt_theta;*/
}
unsigned int *Hough_Fantasy::GetHoughPoint(int theta, int p)
{
    //因为用的是偏移的，所以最后返回霍夫坐标的时候也要记得减去偏移量!
    //p有可能是送进来负数的。这里还可以再加一个边界值判断
    return this->hough_mat_pointer + ((2 * max_p + 1) * (theta + 90) + p + max_p);
}

void Hough_Fantasy::DrawInHoughMat(int x, int y)
{
    //theta的范围
    for (int i = -90; i <= 180; i++)
    {
        //P=xcosθ+ysinθ
        int P = x * cos(((double)i / 180.0) * M_PI) + y * sin(((double)i / 180.0) * M_PI);
        unsigned int *point = GetHoughPoint(i, P);
        if (point != NULL)
        {
            *point = (*point) + 1;
        }
    }
    //以后性能优化可以在这里加个缓存，判断是不是一条线
}

void Hough_Fantasy::DoHoughTransition()
{
    int rows = this->input_mat.rows;
    int cols = this->input_mat.cols;

    //长是θ，而宽是两倍的最大截距
    this->hough_mat_pointer = (unsigned int *)calloc((this->max_p + 1) * 2 * (90 + 180 + 1), sizeof(unsigned int));

    ugly = this->hough_mat_pointer;

    if (this->hough_mat_pointer == NULL)
    {
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < this->input_mat.rows; i++)
    {
        for (int j = 0; j < this->input_mat.cols; j++)
        {
            //遍历目标图像
            uchar *p = GetPoint1(this->input_mat, i, j);
            if (p != NULL)
            {
                //如果p的值大于阈值，则送去做霍夫
                if (*p >= this->threshold)
                {
                    DrawInHoughMat(i, j);
                }
            }
        }
    }
}

//阈值，目前只支持输入灰度图像
Hough_Fantasy::Hough_Fantasy(Mat &input, int threshold)
{
    this->input_mat = input;
    this->threshold = threshold;

    int rows = this->input_mat.rows;
    int cols = this->input_mat.cols;
    this->max_p = (int)(sqrt(rows * rows + cols * cols) + 0.5);
}

Hough_Fantasy::~Hough_Fantasy()
{
    free(this->hough_mat_pointer);
    free(this->sorted_pointer);
}

//自己的阈值检测，最高！
void myThreshold(Mat &input, Mat &output, int val)
{
    input.copyTo(output);
    for (int i = 0; i < input.rows; i++)
    {
        for (int j = 0; j < input.cols; j++)
        {
            uchar *point = GetPoint1(input, i, j);
            if (point != NULL)
            {
                if (*point >= val)
                {
                    *point = 255;
                }
                else
                {
                    *point = 0;
                }
            }
        }
    }
}

int main(int argc, char const *argv[])
{
    Mat image = imread("/home/rinka/Documents/DIP-Fantasy/input/line.png", 1);

    cvtColor(image, image, COLOR_RGB2GRAY);

    Mat temp;
    myThreshold(image, temp, 150);

    namedWindow("Display", WINDOW_AUTOSIZE);
    imshow("Display", temp);
    waitKey(0);

    Hough_Fantasy my_hough(image, 50);

    my_hough.DoHoughTransition();

    int p = 0, theta = 0;
    my_hough.GetOneHoughPoint(p, theta);

    my_hough.PrintLineToImage(image, p, theta, 55);
    my_hough.GetOneHoughPoint(p, theta);
    my_hough.PrintLineToImage(image, p, theta, 55);
    namedWindow("Display2", WINDOW_AUTOSIZE);
    imshow("Display2", image);
    waitKey(0);
    return 0;
}
