#ifndef PRETRAINZM_H
#define PRETRAINZM_H

#include <dirent.h>
#include <iostream>
#include <string.h>

#include <iomanip>
#include <sstream>

#include "FileReader.h"
#include "FileWriter.h"
#include "Constants.h"
#include "../entities/SFC.h"

#include "util.h"

namespace pre_train_zm
{
    void each_cnn(vector<int> sfc, int threshold, int z_value_begin, vector<int> &result, vector<float> &z_values)
    {
        int length = sfc.size();
        int gap = 1;
        // vector<int> result = sfc;
        // vector<float> z_values;
        result = sfc;
        for (size_t i = 0; i < length; i++)
        {
            z_values.push_back(z_value_begin + 1.0 * i / length);
        }
        while (gap <= length)
        {
            vector<int> temp_result;
            vector<float> temp_z_values;
            for (size_t i = 0; i < length; i += gap)
            {
                int temp_sum = 0;
                for (size_t j = 0; j < gap; j++)
                {
                    temp_sum += sfc[j + i];
                }
                if (temp_sum > threshold)
                {
                    return;
                }
                else
                {
                    temp_result.push_back(temp_sum);
                    temp_z_values.push_back(z_value_begin + i * 1.0 / length);
                }
            }
            result = temp_result;
            z_values = temp_z_values;
            gap = gap * 4;
        }
    }

    void cnn(int conv_width, vector<int> sfc, long cardinality, int threshold, vector<float> &z_values, vector<float> &result_cdf)
    {
        // have a large concolution and constantly shrink it until each cell contains n points && n <= threshold
        vector<float> cdf;
        vector<float> keys;
        long N = sfc.size();
        int gap = pow(conv_width, 2);

        vector<int> result_sfc;
        // vector<float> result_cdf;
        // vector<float> z_values;
        long sum = 0;
        int z_value_begin = 0;
        for (size_t i = 0; i < N; i = i + gap)
        {
            vector<int> sub_sfc(sfc.begin() + i, sfc.begin() + i + gap);
            vector<int> approx_sub_sfc;
            vector<float> approx_sub_z_values;
            each_cnn(sub_sfc, threshold, z_value_begin, approx_sub_sfc, approx_sub_z_values);
            result_sfc.insert(result_sfc.end(), approx_sub_sfc.begin(), approx_sub_sfc.end());
            // z_values.insert(z_values.end(), approx_sub_z_values.begin(), approx_sub_z_values.end());
            for (size_t j = 0; j < approx_sub_sfc.size(); j++)
            {
                sum += approx_sub_sfc[j];
                if (sum < cardinality)
                {
                    result_cdf.push_back(sum * 1.0 / cardinality);
                    z_values.push_back(approx_sub_z_values[j]);
                }
            }
            z_value_begin++;
        }
        float z_max = z_values[z_values.size() - 1];
        float z_min = z_values[0];
        float z_gap = z_max - z_min;
        for (size_t i = 0; i < z_values.size(); i++)
        {
            z_values[i] = (z_values[i] - z_min) / z_gap;
        }
    }

    void train_top_level(vector<Point> points)
    {
        auto net = std::make_shared<Net>(1);
#ifdef use_gpu
        net->to(torch::kCUDA);
#endif
        long long N = points.size();
        int bit_num = ceil((log(N)) / log(2)) * 2;
        for (long long i = 0; i < N; i++)
        {
            points[i].x_i = points[i].x * N;
            points[i].y_i = points[i].y * N;
            long long xs[2] = {(long long)points[i].x_i, (long long)points[i].y_i};
            long long curve_val = compute_Z_value(xs, 2, bit_num);
            points[i].curve_val = curve_val;
        }
        sort(points.begin(), points.end(), sort_curve_val());
        long long min_curve_val = points[0].curve_val;
        long long max_curve_val = points[points.size() - 1].curve_val;
        long long gap = max_curve_val - min_curve_val;
        vector<float> locations;
        vector<float> labels;
        for (long long i = 0; i < N; i++)
        {
            points[i].index = i * 1.0 / N;
            points[i].normalized_curve_val = (points[i].curve_val - min_curve_val) * 1.0 / gap;

            locations.push_back(points[i].normalized_curve_val);
            labels.push_back(points[i].index);
        }
        net->train_model(locations, labels);
        net->get_parameters_ZM();
        int max_error = 0;
        int min_error = 0;
        long long total_error = 0;
        for (Point point : points)
        {
            int pos = 0;
            float pred = net->predict_ZM(point.normalized_curve_val);
            pos = pred * N;
            if (pos < 0)
            {
                pos = 0;
            }
            if (pos >= N)
            {
                pos = N - 1;
            }

            long long error = (long long)(point.index * N) - pos;
            total_error += abs(error);
            if (error > 0)
            {
                if (error > max_error)
                {
                    max_error = error;
                }
            }
            else
            {
                if (error < min_error)
                {
                    min_error = error;
                }
            }
        }
        cout << "total_error: " << total_error / N << " max_error: " << max_error << " min_error: " << min_error << endl;
    }

    vector<Point> get_cluster_point(string file_name)
    {
        FileReader filereader(file_name, ",");
        vector<Point> points = filereader.get_points();
        long long N = points.size();
        int bit_num = ceil((log(N)) / log(2)) * 2;
        for (long long i = 0; i < N; i++)
        {
            long long xs[2] = {points[i].x * N, points[i].y * N};
            long long curve_val = compute_Z_value(xs, 2, bit_num);
            points[i].curve_val = curve_val;
        }
        sort(points.begin(), points.end(), sort_curve_val());
        long long min_curve_val = points[0].curve_val;
        long long max_curve_val = points[points.size() - 1].curve_val;
        long long gap = max_curve_val - min_curve_val;
        for (long long i = 0; i < N; i++)
        {
            points[i].index = i * 1.0 / N;
            points[i].normalized_curve_val = (points[i].curve_val - min_curve_val) * 1.0 / gap;
        }
        return points;
    }

    vector<Point> get_rep_set(long long m, int bit_num, long long z_value, vector<Point> all_points)
    {
        // cout<< "get_rep_set: bit_num:" << bit_num << endl;
        // cout<< "get_rep_set: z_value:" << z_value << endl;
        // cout<< "all_points.size(): " << all_points.size() << endl;
        long long N = all_points.size();
        vector<Point> rs;
        if (all_points.size() == 0)
        {
            return rs;
        }

        if (bit_num == 0)
        {
            if (N > 2 * m)
            {
                rs.push_back(all_points[0]);
                for (size_t i = m - 1; i < N; i += m)
                {
                    rs.push_back(all_points[i]);
                }
                rs.push_back(all_points[N - 1]);
            }
            else
            {
                rs.push_back(all_points[N / 2]);
            }
            return rs;
        }
        long long gap = pow(2, bit_num - 2);
        int key_num = pow(2, 2);
        map<int, vector<Point>> points_map;
        // for (size_t i = 0; i < key_num; i++)
        // {
        //     vector<Point> temp;
        //     points_map.insert(pair<int, vector<Point>>(i, temp));
        // }
        for (size_t i = 0; i < N; i++)
        {
            int key = (all_points[i].curve_val - z_value) / gap;
            points_map[key].push_back(all_points[i]);
        }
        all_points.clear();
        all_points.shrink_to_fit();

        map<int, vector<Point>>::iterator iter;
        iter = points_map.begin();
        // while(iter != points_map.end()) {
        //     cout << "map: " << iter->first << " : " << iter->second.size() << endl;
        //     iter++;
        // }
        for (size_t i = 0; i < key_num; i++)
        {
            vector<Point> temp = points_map[i];
            if (temp.size() > 2 * m)
            {
                vector<Point> res = get_rep_set(m, bit_num - 2, z_value + i * gap, temp);
                rs.insert(rs.end(), res.begin(), res.end());
            }
            else if (temp.size() > 0)
            {
                rs.push_back(temp[temp.size() / 2]);
            }
        }
        return rs;
    }

    // get_rep_set_space(m, 0,0,0.5,all_points);
    vector<Point> get_rep_set_space(long long m, double start_x, double start_y, double x_edge_length, double y_edge_length, vector<Point> &all_points)
    {
        // cout << "get_rep_set_space 1" << endl;
        long long N = all_points.size();
        vector<Point> rs;
        if (all_points.size() == 0)
        {
            return rs;
        }
        int key_num = 4;
        map<int, vector<Point>> points_map;
        for (size_t i = 0; i < N; i++)
        {
            int key = 0;
            if (all_points[i].x - start_x <= x_edge_length)
            {
                if (all_points[i].y - start_y <= y_edge_length)
                {
                    key = 0;
                }
                else
                {
                    key = 2;
                }
            }
            else
            {
                if (all_points[i].y - start_y <= y_edge_length)
                {
                    key = 1;
                }
                else
                {
                    key = 3;
                }
            }
            points_map[key].push_back(all_points[i]);
        }
        // cout << "get_rep_set_space 2" << endl;
        // all_points.clear();
        // all_points.shrink_to_fit();

        // map<int, vector<Point>>::iterator iter;
        // iter = points_map.begin();
        // while(iter != points_map.end()) {
        //     cout << "map: " << iter->first << " : " << iter->second.size() << endl;
        //     iter++;
        // }
        // cout << "get_rep_set_space 3" << endl;
        for (size_t i = 0; i < key_num; i++)
        {
            // cout << "get_rep_set_space 4 i= " << i << endl;
            if (points_map[i].size() == 0)
            {
                continue;
            }
            double start_x_temp = start_x;
            double start_y_temp = start_y;
            if (points_map[i].size() > m)
            {
                // cout << "get_rep_set_space 5" << endl;
                if (i == 1)
                {
                    start_x_temp = start_x + x_edge_length;
                }
                if (i == 2)
                {
                    start_y_temp = start_y + y_edge_length;
                }
                if (i == 3)
                {
                    start_x_temp = start_x + x_edge_length;
                    start_y_temp = start_y + y_edge_length;
                }
                vector<Point> res = get_rep_set_space(m, start_x_temp, start_y_temp, x_edge_length / 2, y_edge_length / 2, points_map[i]);
                rs.insert(rs.end(), res.begin(), res.end());
            }
            else if (points_map[i].size() > 0)
            {
                // double x = start_x + x_edge_length / 2;
                // double y = start_y + y_edge_length / 2;
                // if (i == 1)
                // {
                //     x += x_edge_length;
                // }
                // if (i == 2)
                // {
                //     y += y_edge_length;
                // }
                // if (i == 3)
                // {
                //     x += x_edge_length;
                //     y += y_edge_length;
                // }
                // Point point(x, y);
                // int middle_point = (temp.size() - 1) / 2;
                // point.index = temp[middle_point].index;
                // point.normalized_curve_val = temp[middle_point].normalized_curve_val;
                // rs.push_back(point);
                int middle_point = (points_map[i].size() - 1) / 2;
                rs.push_back(points_map[i][middle_point]);
            }
        }
        return rs;
    }

    vector<int> test_Approximate_SFC(string folder, string file_name)
    {
        FileReader filereader(folder + file_name, ",");
        vector<Point> points = filereader.get_points();
        long long N = points.size();
        int bit_num = ceil((log(N)) / log(2));
        long z_value_num = pow(2, bit_num);
        int side = pow(2, bit_num / 2);
        for (int i = 0; i < N; i++)
        {
            points[i].x_i = points[i].x * side;
            points[i].y_i = points[i].y * side;
            long long xs[2] = {points[i].x_i, points[i].y_i};
            long long curve_val = compute_Z_value(xs, 2, bit_num / 2);
            points[i].curve_val = curve_val;
        }
        sort(points.begin(), points.end(), sort_curve_val());
        long index = 0;
        vector<int> sfc;
        long sum = 0;
        for (int i = 0; i < z_value_num; i++)
        {
            int num = 0;
            while (points[index].curve_val == i && index < N)
            {
                index++;
                num++;
            }
            sum += num;
            sfc.push_back(num);
        }
        return sfc;
    }

    // void write_approximate_SFC(string folder, string file_name, int bit_num)
    void write_approximate_SFC(vector<Point> points, string file_name, int bit_num)
    {
        // FileReader filereader(folder + file_name, ",");
        // vector<Point> points = filereader.get_points();
        long long N = points.size();
        int side = pow(2, bit_num / 2);
        vector<long long> sfcs;
        vector<float> features;
        for (int i = 0; i < N; i++)
        {
            long long xs[2] = {points[i].x * side, points[i].y * side};
            long long curve_val = compute_Z_value(xs, 2, bit_num);
            // long long curve_val = compute_Z_value(points[i].x * side, points[i].y * side, bit_num);
            points[i].curve_val = curve_val;
        }
        sort(points.begin(), points.end(), sort_curve_val());
        long long max_curve_val = points[N - 1].curve_val;
        long long min_curve_val = points[0].curve_val;
        long long gap = max_curve_val - min_curve_val;
        for (int i = 0; i < N; i++)
        {
            sfcs.push_back(points[i].curve_val);
            features.push_back((points[i].curve_val - min_curve_val) * 1.0 / gap);
        }
        // cout << "N: " << N << " min_curve_val: " << min_curve_val << " max_curve_val: " << max_curve_val << endl;
        SFC sfc(bit_num, sfcs);
        vector<float> weighted_SFC = sfc.get_weighted_curve();
        // cout << "file_name: " << file_name << endl;
        // cout << "weighted_SFC size: " << weighted_SFC.size() << endl;
        string sfc_weight_path = Constants::SFC_Z_WEIGHT + "bit_num_" + to_string(bit_num) + "/";
        FileWriter SFC_writer(sfc_weight_path);
        SFC_writer.write_weighted_SFC(weighted_SFC, file_name);
    }

    vector<Point> get_points(string folder, string file_name, int resolution)
    {
        FileReader filereader(folder + file_name, ",");
        vector<Point> points = filereader.get_points();
        long long N = points.size();
        cout << "get_points: " << N << endl;
        // cout<< log((N / resolution) * (N / resolution)) << endl;
        int bit_num = 2 * ceil(log((N / resolution)) / log(2));
        cout << "bit_num: " << bit_num << endl;
        for (int i = 0; i < N; i++)
        {
            long long xs[2] = {points[i].x * (N / resolution), points[i].y * (N / resolution)};
            long long curve_val = compute_Z_value(xs, 2, bit_num);
            points[i].curve_val = curve_val;
        }
        sort(points.begin(), points.end(), sort_curve_val());

        long min_curve_val = points[0].curve_val;
        long max_curve_val = points[points.size() - 1].curve_val;
        long gap = max_curve_val - min_curve_val;
        for (long long i = 0; i < N; i++)
        {
            points[i].index = i * 1.0 / N;
            points[i].normalized_curve_val = (points[i].curve_val - min_curve_val) * 1.0 / gap;
        }
        cout << "file_name: " << file_name << endl;
        cout << "N: " << N << " min_curve_val: " << min_curve_val << " max_curve_val: " << max_curve_val << endl;
        return points;
    }

    void train_model_1d(vector<float> features, string folder, string file_name, int resolution, string threshold)
    {
        // TODO generate hist according to normalized_curve_val
        auto net = std::make_shared<Net>(1);
#ifdef use_gpu
        net->to(torch::kCUDA);
#endif
        vector<float> labels;
        long N = features.size();
        sort(features.begin(), features.end());
        for (size_t i = 1; i <= N; i++)
        {
            labels.push_back(i * 1.0 / N);
        }

        file_name = file_name.substr(0, file_name.find(".csv"));
        // cout<< "file_name: " << file_name << endl;
        string features_path = Constants::FEATURES_PATH_ZM + to_string(resolution) + "/" + threshold + "/";
        file_utils::check_dir(features_path);
        FileWriter SFC_writer(features_path);
        SFC_writer.write_SFC(features, file_name + ".csv");
        string model_path = Constants::PRE_TRAIN_MODEL_PATH_ZM + to_string(resolution) + "/" + threshold + "/";
        file_utils::check_dir(model_path);
        model_path = model_path + file_name + ".pt";
        std::ifstream fin(model_path);
        if (!fin)
        {
            net->train_model(features, labels);
            torch::save(net, model_path);
        }
        else
        {
            torch::load(net, model_path);
            net->get_parameters_ZM();
        }
        vector<float> locations1;
        for (size_t i = 0; i < N; i++)
        {
            float pred = net->predict_ZM(features[i]);
            pred = pred < 0 ? 0 : pred;
            pred = pred > 1 ? 1 : pred;
            locations1.push_back(pred);
        }
        SFC_writer.write_SFC(locations1, file_name + "_learned.csv");
    }

    void train_model(vector<Point> points, string folder, string file_name, int resolution, string threshold)
    {
        // TODO generate hist according to normalized_curve_val
        auto net = std::make_shared<Net>(1);
#ifdef use_gpu
        net->to(torch::kCUDA);
#endif
        vector<float> locations;
        vector<float> labels;
        for (Point point : points)
        {
            locations.push_back(point.normalized_curve_val);
            labels.push_back(point.index);
        }

        file_name = file_name.substr(0, file_name.find(".csv"));
        // cout<< "file_name: " << file_name << endl;
        string features_path = Constants::FEATURES_PATH_ZM + to_string(resolution) + "/" + threshold + "/";
        file_utils::check_dir(features_path);
        FileWriter SFC_writer(features_path);
        SFC_writer.write_SFC(locations, file_name + ".csv");
        // cout<< "features_path: " << features_path << endl;
        string model_path = Constants::PRE_TRAIN_MODEL_PATH_ZM + to_string(resolution) + "/" + threshold + "/";
        file_utils::check_dir(model_path);
        // cout<< "model_path: " << model_path << endl;
        model_path = model_path + file_name + ".pt";
        std::ifstream fin(model_path);
        if (!fin)
        {
            net->train_model(locations, labels);
            torch::save(net, model_path);
        }
        else
        {
            torch::load(net, model_path);
            net->get_parameters_ZM();
        }
        vector<float> locations1;
        int N = points.size();
        for (size_t i = 0; i < N; i++)
        {
            float pred = net->predict_ZM(points[i].normalized_curve_val);
            pred = pred < 0 ? 0 : pred;
            pred = pred > 1 ? 1 : pred;
            locations1.push_back(pred);
        }
        SFC_writer.write_SFC(locations1, file_name + "learned.csv");
    }

    // void test_errors(string file_name_t, int resolution)
    // {
    //     // string path = "OSM_100000000_1_2_.csv";
    //     file_name_t = "uniform_1000000_1_2_.csv";
    //     // train target to compare.
    //     cout << "file name: " << file_name_t << endl;
    //     string file_name = file_name_t.substr(0, file_name_t.find(".csv"));
    //     string model_path = Constants::PRE_TRAIN_MODEL_PATH_ZM + to_string(resolution) + "/" + file_name + ".pt";
    //     std::ifstream fin(model_path);
    //     string floder = "/home/liuguanli/Documents/datasets/RLRtree/raw/";
    //     vector<Point> points = get_points(floder, file_name_t, resolution);

    //     std::stringstream stream;
    //     stream << std::fixed << std::setprecision(1) << exp_re;
    //     string threshold = stream.str();

    //     if (!fin)
    //     {
    //         pre_train_zm::train_model(points, floder, file_name_t, resolution, threshold);
    //     }
    //     else
    //     {
    //         vector<float> locations;
    //         for (Point point : points)
    //         {
    //             locations.push_back(point.normalized_curve_val);
    //         }
    //         string features_path = Constants::FEATURES_PATH_ZM + to_string(resolution) + "/";
    //         FileWriter SFC_writer(features_path);
    //         SFC_writer.write_SFC(locations, file_name + ".csv");
    //     }

    //     // string ppath = "pre_train/models_zm/" + to_string(1) + "/";
    //     // struct dirent *ptr;
    //     // DIR *dir;
    //     // dir = opendir(ppath.c_str());
    //     // while ((ptr = readdir(dir)) != NULL)
    //     // {
    //     //     if (ptr->d_name[0] == '.')
    //     //         continue;
    //     //     string file_name_s = ptr->d_name;
    //     //     int find_result = file_name_s.find(".pt");
    //     //     if (find_result > 0 && find_result <= file_name_s.length())
    //     //     {
    //     //         auto net = std::make_shared<Net>(1);
    //     //         model_path = "pre_train/models_zm/1/" + file_name_s;
    //     //         torch::load(net, model_path);
    //     //         net->get_parameters_ZM();
    //     //         // net->print_parameters();
    //     //         double errs = 0;
    //     //         int N = points.size();
    //     //         for (size_t i = 0; i < N; i++)
    //     //         {
    //     //             float pred = net->predict_ZM(points[i].normalized_curve_val);
    //     //             // cout<< "pred: " << pred << endl;
    //     //             if (pred < 0)
    //     //             {
    //     //                 pred = 0;
    //     //             }
    //     //             if (pred > 1)
    //     //             {
    //     //                 pred = 1;
    //     //             }
    //     //             // cout<< "error:" << abs(pred - points[i].index) << endl;
    //     //             errs += abs(pred - points[i].index);
    //     //             // if (i % 1000000 == 0)
    //     //             // {
    //     //             //     cout<< "pred: " << pred << " index: " << points[i].index << " abs: " << abs(pred - points[i].index) << " errs: " << errs << endl;
    //     //             // }
    //     //             // break;
    //     //         }
    //     //         // 1.67772e+07 10,0000000
    //     //         cout << file_name_s << " errs: " << errs << endl;
    //     //     }
    //     //     // break;
    //     // }
    // }

    void pre_train_1d_Z(int resolution, string threshold)
    {
        // std::stringstream stream;
        // stream << std::fixed << std::setprecision(1) << threshold;
        string ppath = Constants::PRE_TRAIN_1D_DATA + threshold + "/";
        cout << "ppath:" << ppath << endl;
        struct dirent *ptr;
        DIR *dir;
        dir = opendir(ppath.c_str());
        while ((ptr = readdir(dir)) != NULL)
        {
            if (ptr->d_name[0] == '.')
                continue;
            string path = ptr->d_name;
            int find_result = path.find(".csv");
            if (find_result > 0 && find_result <= path.length())
            {
                FileReader filereader(ppath + path, ",");
                vector<float> features = filereader.read_features();
                train_model_1d(features, ppath, path, resolution, threshold);
            }
        }
    }

    // void pre_train_multid_Z(int resolution, string threshold)
    // {
    //     string ppath = Constants::PRE_TRAIN_DATA + threshold + "/";
    //     struct dirent *ptr;
    //     DIR *dir;
    //     dir = opendir(ppath.c_str());
    //     while ((ptr = readdir(dir)) != NULL)
    //     {
    //         if (ptr->d_name[0] == '.')
    //             continue;
    //         string path = ptr->d_name;
    //         int find_result = path.find(".csv");
    //         if (find_result > 0 && find_result <= path.length())
    //         {
    //             train_model(get_points(ppath, path, resolution), ppath, path, resolution, threshold);
    //         }
    //     }
    // }
    // std::shared_ptr<Net>
    map<string, vector<float>> distributions = {
        {"normal", {1, 0, 0}}, {"skewed", {0, 1, 0}}, {"uniform", {0, 0, 1}}};
    map<int, vector<float>> methods = {
        {1, {0, 1, 0, 0, 0, 0}}, {2, {0, 0, 1, 0, 0, 0}}, {3, {0, 0, 0, 1, 0, 0}}, {4, {0, 0, 0, 0, 1, 0}}, {5, {0, 0, 0, 0, 0, 1}}};
    auto query_cost_model = std::make_shared<Net>(10, 32);
    auto build_cost_model = std::make_shared<Net>(10, 32);

    void cost_model_build()
    {
        FileReader filereader(",");
        // string path = "/home/liuguanli/Dropbox/shared/VLDB20/codes/rsmi/cost_model/train_set_formatted_normalized.csv";
        // string build_time_model_path = "/home/liuguanli/Dropbox/shared/VLDB20/codes/rsmi/cost_model/build_time_model_zm_normalized.pt";
        // string query_time_model_path = "/home/liuguanli/Dropbox/shared/VLDB20/codes/rsmi/cost_model/query_time_model_zm_normalized.pt";
        string path = "/home/cwang39/RSMI/cost_model/train_set_formatted.csv";
        string build_time_model_path = "/home/cwang39/RSMI/cost_model/build_time_model_zm.pt";
        string query_time_model_path = "/home/cwang39/RSMI/cost_model/query_time_model_zm.pt";

        std::ifstream fin_build(build_time_model_path);
        std::ifstream fin_query(query_time_model_path);
        if (fin_build && fin_query)
        {
            torch::load(build_cost_model, build_time_model_path);
            torch::load(query_cost_model, query_time_model_path);
        }
        else
        {
            vector<float> parameters;
            vector<float> build_time_labels;
            vector<float> query_time_labels;
            filereader.get_cost_model_data(path, parameters, build_time_labels, query_time_labels);
#ifdef use_gpu
            query_cost_model->to(torch::kCUDA);
            build_cost_model->to(torch::kCUDA);
#endif
            build_cost_model->train_model(parameters, build_time_labels);
            query_cost_model->train_model(parameters, query_time_labels);
            torch::save(build_cost_model, build_time_model_path);
            torch::save(query_cost_model, query_time_model_path);

            int training_set_size = build_time_labels.size();

            // TODO evaluate
// lambda: 0 build accuracy: 0.97619
// lambda: 0 query accuracy: 0.771429
// lambda: 0 total accuracy: 0.771429
// lambda: 0.1 build accuracy: 0.97619
// lambda: 0.1 query accuracy: 0.771429
// lambda: 0.1 total accuracy: 0.771429
// lambda: 0.2 build accuracy: 0.97619
// lambda: 0.2 query accuracy: 0.771429
// lambda: 0.2 total accuracy: 0.747619

// lambda: 0.3 build accuracy: 0.97619
// lambda: 0.3 query accuracy: 0.771429
// lambda: 0.3 total accuracy: 0.752381

// lambda: 0.4 build accuracy: 0.97619
// lambda: 0.4 query accuracy: 0.771429
// lambda: 0.4 total accuracy: 0.742857

// lambda: 0.5 build accuracy: 0.97619
// lambda: 0.5 query accuracy: 0.771429
// lambda: 0.5 total accuracy: 0.8

// lambda: 0.6 build accuracy: 0.97619
// lambda: 0.6 query accuracy: 0.771429
// lambda: 0.6 total accuracy: 0.828571

// lambda: 0.7 build accuracy: 0.97619
// lambda: 0.7 query accuracy: 0.771429
// lambda: 0.7 total accuracy: 0.928571

// lambda: 0.8 build accuracy: 0.97619
// lambda: 0.8 query accuracy: 0.771429
// lambda: 0.8 total accuracy: 0.990476

// lambda: 0.9 build accuracy: 0.97619
// lambda: 0.9 query accuracy: 0.771429
// lambda: 0.9 total accuracy: 1
// lambda: 1 build accuracy: 0.97619
// lambda: 1 query accuracy: 0.771429
// lambda: 1 total accuracy: 1
            // torch::Tensor x = torch::tensor(parameters, at::kCUDA).reshape({build_time_labels.size(), 10});
            vector<float> build_scores = build_cost_model->predict(parameters);
            vector<float> query_scores = query_cost_model->predict(parameters);

            int cardinalitys[] = {1, 2, 4, 8, 16, 32, 64, 128, 200, 400, 800, 1600, 3200, 6400};
            for (size_t j = 0; j < 11; j++)
            {
                float lambda = j * 0.1;
                int total_num = 0;
                int correct_num = 0;
                int correct_num_build = 0;
                int correct_num_query = 0;
                for (size_t i = 0; i < 14; i++)
                {
                    map<string, vector<float>>::iterator out_iter;
                    out_iter = distributions.begin();
                    while (out_iter != distributions.end())
                    {
                        float max_score = 0;
                        float max_build_score = 0;
                        float max_query_score = 0;
                        float max_score_real = 0;
                        float max_build_score_real = 0;
                        float max_query_score_real = 0;
                        int predicted_result = 1;
                        int predicted_build_result = 0;
                        int predicted_query_result = 1;
                        int real_result = 1;
                        int real_build_result = 0;
                        int real_query_result = 1;
                        vector<float> items;
                        vector<float> distribution_list = distributions[out_iter->first];
                        items.push_back(cardinalitys[i] * 1.0 / 6400);
                        items.insert(items.end(), distribution_list.begin(), distribution_list.end());
                        map<int, vector<float>>::iterator iter;
                        iter = methods.begin();
                        while (iter != methods.end())
                        {
                            vector<float> temp = items;
                            temp.insert(temp.end(), iter->second.begin(), iter->second.end());
#ifdef use_gpu
                            torch::Tensor x = torch::tensor(temp, at::kCUDA).reshape({1, 10});
#else
                            torch::Tensor x = torch::tensor(temp).reshape({1, 10});
#endif
                            float build_score = build_cost_model->predict(x).item().toFloat();
                            float query_score = query_cost_model->predict(x).item().toFloat();
                            float score = lambda * build_score + (1 - lambda) * query_score;
                            // cout<< "build_score: " <<build_score << " query_score: " << query_score << endl;
                            if (build_score > max_build_score)
                            {
                                max_build_score = build_score;
                                predicted_build_result = iter->first;
                            }
                            if (query_score > max_query_score)
                            {
                                max_query_score = query_score;
                                predicted_query_result = iter->first;
                            }
                            if (score > max_score)
                            {
                                max_score = score;
                                predicted_result = iter->first;
                            }

                            // find from oridinal data set
                            for (size_t k = 0; k < training_set_size; k++)
                            {
                                vector<float> training_set_temp(parameters.begin() + k * 10, parameters.begin() + k * 10 + 10);
                                if (training_set_temp == temp)
                                {
                                    float build_score_real = build_time_labels[k];
                                    float query_score_real = query_time_labels[k];
                                    float score_real = lambda * build_score_real + (1 - lambda) * query_score_real;
                                    if (build_score_real > max_build_score_real)
                                    {
                                        max_build_score_real = build_score_real;
                                        real_build_result = iter->first;
                                    }
                                    if (query_score_real > max_query_score_real)
                                    {
                                        max_query_score_real = query_score_real;
                                        real_query_result = iter->first;
                                    }
                                    if (score_real > max_score_real)
                                    {
                                        max_score_real = score_real;
                                        real_result = iter->first;
                                    }
                                }
                            }
                            // cout << "real_result: " << real_result << " predicted_result: " << predicted_result << endl;
                            if (real_result == predicted_result)
                            {
                                correct_num++;
                            }
                            if (real_build_result == predicted_build_result)
                            {
                                correct_num_build++;
                            }
                            if (real_query_result == predicted_query_result)
                            {
                                correct_num_query++;
                            }
                            total_num++;

                            iter++;
                        }
                        out_iter++;
                    }
                }
                cout<< "lambda: " << lambda << " build accuracy: " << correct_num_build * 1.0 / total_num << endl;
                cout<< "lambda: " << lambda << " query accuracy: " << correct_num_query * 1.0 / total_num << endl;
                cout<< "lambda: " << lambda << " total accuracy: " << correct_num * 1.0 / total_num << endl;
            }
        }
    }

    string get_distribution(Histogram hist)
    {
        Histogram uniform = Net::pre_trained_histograms["index_446"];
        Histogram normal = Net::pre_trained_histograms["index_446"];

        double distance = 0;
        distance = hist.cal_similarity(uniform.hist);
        if (distance < 0.1)
        {
            return "uniform";
        }
        distance = hist.cal_similarity(normal.hist);
        if (distance < 0.1)
        {
            return "normal";
        }
        return "skewed";
    }

    void cost_model_predict(ExpRecorder &exp_recorder, float lambda, long cardinality, string distribution)
    {
        // cout << "cardinality: " << cardinality << endl;
        // cout << "distribution: " << distribution << endl;

        vector<float> distribution_list = distributions[distribution];
        // {"cl", {1, 0, 0, 0, 0, 0}},

        float score = 0;
        vector<float> parameters;

        parameters.push_back(cardinality * 1.0 / 6400);
        parameters.insert(parameters.end(), distribution_list.begin(), distribution_list.end());

        map<int, vector<float>>::iterator iter;
        iter = methods.begin();

        auto start = chrono::high_resolution_clock::now();
        // float min_cost = 100;
        float max_score = 0;
        int result = 1;
        // mr = 1
        // or = 2
        // rl = 3
        // rs = 4
        // sp = 5
        // iter->first: 1 build_score: 5.98972 query_score: 1.05207 score: 4.01466
        // iter->first: 2 build_score: 2.95342 query_score: 2.76243 score: 2.87702
        // iter->first: 3 build_score: 1.70991 query_score: 4.97592 score: 3.01632
        // iter->first: 4 build_score: 4.05757 query_score: 5.58559 score: 4.66878
        // iter->first: 5 build_score: 4.99554 query_score: 3.02659 score: 4.20796
        while (iter != methods.end())
        {
            vector<float> temp = parameters;
            temp.insert(temp.end(), iter->second.begin(), iter->second.end());
#ifdef use_gpu
            torch::Tensor x = torch::tensor(temp, at::kCUDA).reshape({1, 10});
#else
            torch::Tensor x = torch::tensor(temp).reshape({1, 10});
#endif
            float build_score = build_cost_model->predict(x).item().toFloat();
            float query_score = query_cost_model->predict(x).item().toFloat();
            score = lambda * build_score + (1 - lambda) * query_score;
            // cout<< "build_score: " <<build_score << " query_score: " << query_score << endl;
            if (score > max_score)
            {
                max_score = score;
                result = iter->first;
            }
            // cout << "iter->first: " << iter->first << " build_score: " << build_score << " query_score: " << query_score << " score: " << score << endl;
            iter++;
        }
        // cout<< "result: " << result << endl;
        switch (result)
        {
        case 1: // mr = 1
            exp_recorder.test_model_reuse();
            break;
        case 2: // or = 2
            exp_recorder.test_reset();
            break;
        case 3: // rl = 3
            exp_recorder.test_rl();
            break;
        case 4: // rs = 4
            exp_recorder.test_rs();
            break;
        case 5: // sp = 5
            exp_recorder.test_sp();
            break;
        default:
            exp_recorder.test_reset();
            break;
        }
        auto finish = chrono::high_resolution_clock::now();
        exp_recorder.cost_model_time += chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
    }

}; // namespace pre_train

#endif