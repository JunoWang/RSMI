#ifndef PRETRAINRSMI_H
#define PRETRAINRSMI_H

#include <dirent.h>
#include <iostream>
#include <string.h>

#include "FileReader.h"
#include "FileWriter.h"

#include "util.h"

namespace pre_train_rsmi
{
    vector<Point> get_points(string folder, string file_name, int resolution, string type)
    {
        FileReader filereader(folder + file_name, ",");
        vector<Point> points = filereader.get_points();
        long long N = points.size();
        // long long side = ceil(log(N / resolution) / log(2));
        long long side = pow(2, ceil(log(N / resolution) / log(2)));
        // cout << "side: " << side << endl;
        sort(points.begin(), points.end(), sortX());
        for (int i = 0; i < N; i++)
        {
            points[i].x_i = i / resolution;
        }
        sort(points.begin(), points.end(), sortY());
        if (type == "Z")
        {
            for (int i = 0; i < N; i++)
            {
                points[i].y_i = i / resolution;
                long long xs[2] = {(long long)points[i].x * N, (long long)points[i].y * N};
                points[i].curve_val = compute_Z_value(xs, 2, side);
            }
        }
        if (type == "H")
        {
            for (int i = 0; i < N; i++)
            {
                points[i].y_i = i / resolution;
                points[i].curve_val = compute_Hilbert_value(points[i].x_i, points[i].y_i, side);
            }
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
        // cout<< "file_name: " << file_name << endl;
        // cout<< "N: " << N << " min_curve_val: " << min_curve_val << " max_curve_val: " << max_curve_val << endl;
        return points;
    }
    vector<Point> get_points(string folder, string file_name, int resolution)
    {
        return get_points(folder, file_name, resolution, "H");
    }

    void train_model(vector<Point> points, string folder, string file_name, int resolution, string type)
    {
        // TODO generate hist according to normalized_curve_val
        auto net = std::make_shared<Net>(2);
#ifdef use_gpu
        net->to(torch::kCUDA);
#endif
        vector<float> locations;
        vector<float> labels;
        vector<float> features;
        for (Point point : points)
        {
            features.push_back(point.normalized_curve_val);
            locations.push_back(point.x);
            locations.push_back(point.y);
            labels.push_back(point.index);
        }
        file_name = file_name.substr(0, file_name.find(".csv"));
        // cout<< "file_name: " << file_name << endl;
        string features_path = Constants::FEATURES_PATH_RSMI + to_string(resolution) + "/" + type + "/";
        FileWriter SFC_writer(features_path);
        SFC_writer.write_SFC(features, file_name + ".csv");

        string model_path = Constants::PRE_TRAIN_MODEL_PATH_RSMI + to_string(resolution) + "/" + type + "/" + file_name + ".pt";
        std::ifstream fin(model_path);
        if (!fin)
        {
            net->train_model(locations, labels);
            torch::save(net, model_path);
        }
        else
        {
            torch::load(net, model_path);
            net->get_parameters();
        }
        // vector<float> locations1;
        // int N = points.size();
        // for (size_t i = 0; i < N; i++)
        // {
        //     float pred = net->predict(points[i]);
        //     pred = pred < 0 ? 0 : pred;
        //     pred = pred > 1 ? 1 : pred;
        //     locations1.push_back(pred);
        // }
        // SFC_writer.write_SFC(locations1, file_name + "learned.csv");
    }

    void train_for_cost_model(vector<Point> points, string folder, string file_name, int resolution, string type)
    {
        // TODO generate hist according to normalized_curve_val
        auto net = std::make_shared<Net>(2);
#ifdef use_gpu
        net->to(torch::kCUDA);
#endif
        vector<float> locations;
        vector<float> labels;
        vector<float> features;
        for (Point point : points)
        {
            features.push_back(point.normalized_curve_val);
            locations.push_back(point.x);
            locations.push_back(point.y);
            labels.push_back(point.index);
        }
        file_name = file_name.substr(0, file_name.find(".csv"));
        // cout<< "file_name: " << file_name << endl;
        string features_path = Constants::FEATURES_PATH_RSMI + to_string(resolution) + "/" + type + "/";
        FileWriter SFC_writer(features_path);
        SFC_writer.write_SFC(features, file_name + ".csv");

        string model_path = Constants::PRE_TRAIN_MODEL_PATH_RSMI + to_string(resolution) + "/" + type + "/" + file_name + ".pt";
        std::ifstream fin(model_path);
        // if (!fin)
        // {
        net->train_model(locations, labels);
        torch::save(net, model_path);
        // }
        // else
        // {
        // torch::load(net, model_path);
        // net->get_parameters();
        // }
        // vector<float> locations1;
        // int N = points.size();
        // for (size_t i = 0; i < N; i++)
        // {
        //     float pred = net->predict(points[i]);
        //     pred = pred < 0 ? 0 : pred;
        //     pred = pred > 1 ? 1 : pred;
        //     locations1.push_back(pred);
        // }
        // SFC_writer.write_SFC(locations1, file_name + "learned.csv");
    }

    void pre_train_2d_H(int resolution, string threshold)
    {
        string ppath = Constants::PRE_TRAIN_DATA + "H/";
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
                train_model(get_points(ppath, path, resolution), ppath, path, resolution, "H");
            }
        }
    }

    void pre_train_2d_Z(int resolution, string threshold)
    {
        string ppath = Constants::PRE_TRAIN_DATA + "H/";
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
                train_model(get_points(ppath, path, resolution, "Z"), ppath, path, resolution, "Z");
            }
        }
    }

    void test_errors(vector<Point> points, auto net, long &max_error, long &min_error)
    {
        int N = points.size();
        long error = 0;
        for (size_t i = 0; i < N; i++)
        {
            float pred = net->predict(points[i]) * N;
            pred = pred < 0 ? 0 : pred;
            pred = pred >= N ? N - 1 : pred;
            error = points[i].index * N - pred;
            if (error > 0)
            {
                max_error = error > max_error ? error : max_error;
            }
            else
            {
                min_error = error < min_error ? error : min_error;
            }
        }
    }

    void test_errors(vector<Point> points, string model_path, string type, long &max_error, long &min_error)
    {
        auto net = std::make_shared<Net>(2);
#ifdef use_gpu
        net->to(torch::kCUDA);
#endif
        // std::ifstream fin(model_path);
        // if (fin)
        // {
        torch::load(net, model_path);
        net->get_parameters();
        // }
        int N = points.size();
        long error = 0;
        for (size_t i = 0; i < N; i++)
        {
            float pred = net->predict(points[i]) * N;
            pred = pred < 0 ? 0 : pred;
            pred = pred >= N ? N - 1 : pred;
            error = points[i].index * N - pred;
            if (error > 0)
            {
                max_error = error > max_error ? error : max_error;
            }
            else
            {
                min_error = error < min_error ? error : min_error;
            }
        }
    }

    void test_point_query(vector<Point> points, auto net, long max_error, long min_error)
    {
        int N = points.size();
        for (size_t i = 0; i < N; i++)
        {
            Point query_point = points[i];
            float pred = net->predict(query_point) * N;
            long front = pred + min_error;
            front = front < 0 ? 0 : front;
            long back = pred + max_error;
            back = back >= N ? N - 1 : back;
            while (front <= back)
            {
                long mid = (front + back) / 2;
                Point point = points[mid];
                if (point.normalized_curve_val > query_point.normalized_curve_val)
                {
                    back = mid - 1;
                }
                else if (point.normalized_curve_val < query_point.normalized_curve_val)
                {
                    front = mid + 1;
                }
                else
                {
                    break;
                }
            }
        }
    }

    void test_point_query(vector<Point> points, string model_path, string type, long max_error, long min_error)
    {
        auto net = std::make_shared<Net>(2);
#ifdef use_gpu
        net->to(torch::kCUDA);
#endif
        //         file_name = file_name.substr(0, file_name.find(".csv"));
        //         string model_path = Constants::PRE_TRAIN_MODEL_PATH_RSMI + to_string(1) + "/" + type + "/" + file_name + ".pt";
        torch::load(net, model_path);
        net->get_parameters();
        int N = points.size();

        for (size_t i = 0; i < N; i++)
        {
            Point query_point = points[i];
            float pred = net->predict(query_point) * N;
            long front = pred + min_error;
            front = front < 0 ? 0 : front;
            long back = pred + max_error;
            back = back >= N ? N - 1 : back;
            while (front <= back)
            {
                long mid = (front + back) / 2;
                Point point = points[mid];
                if (point.normalized_curve_val > query_point.normalized_curve_val)
                {
                    back = mid - 1;
                }
                else if (point.normalized_curve_val < query_point.normalized_curve_val)
                {
                    front = mid + 1;
                }
                else
                {
                    break;
                }
            }
        }
    }

    // vector<Point> get_rep_set_space(long long m, double start_x, double start_y, double edge_length, vector<Point> all_points)
    // {
    //     long long N = all_points.size();
    //     vector<Point> rs;
    //     if (all_points.size() == 0)
    //     {
    //         return rs;
    //     }
    //     int key_num = 4;
    //     map<int, vector<Point>> points_map;
    //     for (size_t i = 0; i < N; i++)
    //     {
    //         int key = 0;
    //         if (all_points[i].x - start_x <= edge_length)
    //         {
    //             if (all_points[i].y - start_y <= edge_length)
    //             {
    //                 key = 0;
    //             }
    //             else
    //             {
    //                 key = 2;
    //             }
    //         }
    //         else
    //         {
    //             if (all_points[i].y - start_y <= edge_length)
    //             {
    //                 key = 1;
    //             }
    //             else
    //             {
    //                 key = 3;
    //             }
    //         }
    //         points_map[key].push_back(all_points[i]);
    //     }
    //     all_points.clear();
    //     all_points.shrink_to_fit();

    //     map<int, vector<Point>>::iterator iter;
    //     iter = points_map.begin();
    //     // while(iter != points_map.end()) {
    //     //     cout << "map: " << iter->first << " : " << iter->second.size() << endl;
    //     //     iter++;
    //     // }
    //     for (size_t i = 0; i < key_num; i++)
    //     {
    //         vector<Point> temp = points_map[i];
    //         double start_x_temp = start_x;
    //         double start_y_temp = start_y;
    //         if (temp.size() > 2 * m)
    //         {
    //             if (i == 1)
    //             {
    //                 start_x_temp = start_x + edge_length;
    //             }
    //             if (i == 2)
    //             {
    //                 start_y_temp = start_y + edge_length;
    //             }
    //             if (i == 3)
    //             {
    //                 start_x_temp = start_x + edge_length;
    //                 start_y_temp = start_y + edge_length;
    //             }
    //             vector<Point> res = get_rep_set_space(m, start_x_temp, start_y_temp, edge_length / 2, temp);
    //             rs.insert(rs.end(), res.begin(), res.end());
    //         }
    //         else if (temp.size() > 0)
    //         {
    //             // TODO use fake points!!!
    //             double x = start_x + edge_length / 2;
    //             double y = start_y + edge_length / 2;
    //             if (i == 1)
    //             {
    //                 x += edge_length;
    //             }
    //             if (i == 2)
    //             {
    //                 y += edge_length;
    //             }
    //             if (i == 3)
    //             {
    //                 x += edge_length;
    //                 y += edge_length;
    //             }
    //             Point point(x, y);
    //             rs.push_back(point);
    //         }
    //     }
    //     return rs;
    // }

    void pre_train_cost_model(string method)
    {
        string ppath = Constants::PRE_TRAIN_DATA;
        struct dirent *ptr;
        DIR *dir;
        dir = opendir(ppath.c_str());
        string threshold = "0.1";
        if (method == "mr")
        {
            // pre_train_rsmi::pre_train_2d_H(Constants::RESOLUTION, threshold);
            pre_train_rsmi::pre_train_2d_Z(Constants::RESOLUTION, threshold);
            Net::load_pre_trained_model_rsmi(threshold);
        }

        FileWriter file_writer("/home/cwang/RSMI/cost_model/");
        while ((ptr = readdir(dir)) != NULL)
        {
            if (ptr->d_name[0] == '.')
                continue;
            string path = ptr->d_name;
            int find_result = path.find(".csv");
            if (find_result > 0 && find_result <= path.length())
            {
                cout << "path: " << path << endl;
                auto start = chrono::high_resolution_clock::now();
                vector<Point> all_points = get_points(ppath, path, 1, "Z");
                auto finish = chrono::high_resolution_clock::now();
                long read_data_time = chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
                long max_error = 0;
                long min_error = 0;
                string file_name = path.substr(0, path.find(".csv"));
                string model_path = Constants::PRE_TRAIN_MODEL_PATH_RSMI + to_string(1) + "/" + "Z" + "/" + file_name + ".pt";
                int N = all_points.size();
                auto net = std::make_shared<Net>(2);
                cout << "all_points size: " << N << endl;
                cout << "read_data_time: " << read_data_time << endl;

                size_t pos = path.find("_");
                string distribution = path.substr(0, pos);
                double build_time = 0;
                if (method == "or") // original
                {
                    start = chrono::high_resolution_clock::now();
                    train_for_cost_model(all_points, ppath, path, 1, "Z");
                    test_errors(all_points, model_path, "Z", max_error, min_error);
                    cout << "max_error: " << max_error << endl;
                    cout << "min_error: " << min_error << endl;
                    finish = chrono::high_resolution_clock::now();
                    build_time = read_data_time + chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
                    cout << "build_time: " << build_time << endl;
                }
                else if (method == "rs")
                {
                    start = chrono::high_resolution_clock::now();
                    vector<Point> rs_points = pre_train_zm::get_rep_set_space(sqrt(N), 0, 0, 0.5, 0.5, all_points);
                    train_for_cost_model(rs_points, ppath, path, 1, "Z");
                    test_errors(all_points, model_path, "Z", max_error, min_error);
                    cout << "max_error: " << max_error << endl;
                    cout << "min_error: " << min_error << endl;
                    finish = chrono::high_resolution_clock::now();
                    build_time = read_data_time + chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
                    cout << "build_time: " << build_time << endl;
                }
                else if (method == "sp")
                {
                    start = chrono::high_resolution_clock::now();
                    vector<Point> sp_points;
                    double sampling_rate = 0.01;
                    int sample_gap = 1 / sampling_rate;
                    long long counter = 0;
                    for (Point point : all_points)
                    {
                        if (counter % sample_gap == 0)
                        {
                            sp_points.push_back(point);
                        }
                        counter++;
                    }
                    train_for_cost_model(sp_points, ppath, path, 1, "Z");
                    test_errors(all_points, model_path, "Z", max_error, min_error);
                    cout << "max_error: " << max_error << endl;
                    cout << "min_error: " << min_error << endl;
                    finish = chrono::high_resolution_clock::now();
                    build_time = read_data_time + chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
                    cout << "build_time: " << build_time << endl;
                }
                else if (method == "mr")
                {
                    start = chrono::high_resolution_clock::now();
                    vector<float> locations;
                    auto start_mr = chrono::high_resolution_clock::now();
                    for (Point point : all_points)
                    {
                        locations.push_back(point.normalized_curve_val);
                    }
                    Histogram histogram(pow(2, Constants::UNIFIED_Z_BIT_NUM), locations);
                    net->is_reusable_rsmi_Z(histogram, threshold, model_path);
                    cout << "model_path: " << model_path << endl;
                    test_errors(all_points, model_path, "Z", max_error, min_error);
                    finish = chrono::high_resolution_clock::now();
                    build_time = chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
                    cout << "build_time: " << build_time << endl;
                }
                else if (method == "cl")
                {
                    start = chrono::high_resolution_clock::now();
                    int k = sqrt(N);
                    string commandStr = "python /home/liuguanli/Dropbox/shared/VLDB20/codes/rsmi/cluster/cluster_for_cost.py -d " +
                                        file_name + " -k " + to_string(k) +
                                        " -f /home/liuguanli/Documents/pre_train/cluster/%s_minibatchkmeans_auto.csv";
                    // string commandStr = "python /home/liuguanli/Documents/pre_train/rl_4_sfc/RL_4_SFC.py";
                    cout << "commandStr: " << commandStr << endl;
                    char command[1024];
                    strcpy(command, commandStr.c_str());
                    int res = system(command);
                    vector<Point> cl_points = pre_train_zm::get_cluster_point("/home/liuguanli/Documents/pre_train/cluster/" + file_name + "_minibatchkmeans_auto.csv");

                    train_for_cost_model(cl_points, ppath, path, 1, "Z");
                    test_errors(all_points, model_path, "Z", max_error, min_error);
                    cout << "max_error: " << max_error << endl;
                    cout << "min_error: " << min_error << endl;
                    finish = chrono::high_resolution_clock::now();
                    build_time = read_data_time + chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
                }
                else if (method == "rl")
                {
                    start = chrono::high_resolution_clock::now();
                    int bit_num = Constants::BIT_NUM;
                    // pre_train_zm::write_approximate_SFC(Constants::DATASETS, exp_recorder.distribution + "_" + to_string(exp_recorder.dataset_cardinality) + "_" + to_string(exp_recorder.skewness) + "_2_.csv", bit_num);
                    pre_train_zm::write_approximate_SFC(all_points, path, bit_num);
                    string commandStr = "python /home/cwang39/RSMI/pre_train/rl_4_sfc/RL_4_SFC_for_cost_model.py -d " +
                                        file_name + " -b " + to_string(bit_num) +
                                        " -f /home/cwang39/RSMI/pre_train/sfc_z_weight/bit_num_%d/%s.csv";
                    // string commandStr = "python /home/liuguanli/Documents/pre_train/rl_4_sfc/RL_4_SFC.py";
                    char command[1024];
                    strcpy(command, commandStr.c_str());
                    int res = system(command);
                    // // todo save data
                    vector<int> sfc;
                    vector<float> labels;
                    vector<float> locations;
                    FileReader RL_SFC_reader("", ",");
                    // labels.push_back(0);
                    RL_SFC_reader.read_sfc("/home/liuguanli/Documents/pre_train/sfc_z_weight/bit_num_" + to_string(bit_num) + "/" + path, sfc, labels);

                    double xs[] = {0.5, 1.5, 0.5, 1.5, 2.5, 3.5, 2.5, 3.5, 0.5, 1.5, 0.5, 1.5, 2.5, 3.5, 2.5, 3.5, 4.5, 5.5, 4.5, 5.5, 6.5, 7.5, 6.5, 7.5, 4.5, 5.5, 4.5, 5.5, 6.5, 7.5, 6.5, 7.5, 0.5, 1.5, 0.5, 1.5, 2.5, 3.5, 2.5, 3.5, 0.5, 1.5, 0.5, 1.5, 2.5, 3.5, 2.5, 3.5, 4.5, 5.5, 4.5, 5.5, 6.5, 7.5, 6.5, 7.5, 4.5, 5.5, 4.5, 5.5, 6.5, 7.5, 6.5, 7.5};
                    double ys[] = {0.5, 0.5, 1.5, 1.5, 0.5, 0.5, 1.5, 1.5, 2.5, 2.5, 3.5, 3.5, 2.5, 2.5, 3.5, 3.5, 0.5, 0.5, 1.5, 1.5, 0.5, 0.5, 1.5, 1.5, 2.5, 2.5, 3.5, 3.5, 2.5, 2.5, 3.5, 3.5, 4.5, 4.5, 5.5, 5.5, 4.5, 4.5, 5.5, 5.5, 6.5, 6.5, 7.5, 7.5, 6.5, 6.5, 7.5, 7.5, 4.5, 4.5, 5.5, 5.5, 4.5, 4.5, 5.5, 5.5, 6.5, 6.5, 7.5, 7.5, 6.5, 6.5, 7.5, 7.5};
                    for (size_t i = 0; i < 64; i++)
                    {
                        locations.push_back(xs[i] / 8);
                        locations.push_back(ys[i] / 8);
                    }
#ifdef use_gpu
                    net->to(torch::kCUDA);
#endif
                    cout << "labels: " << labels.size() << endl;
                    cout << "locations: " << locations.size() << endl;
                    if (labels.size() != 64)
                    {
                        continue;
                    }

                    net->train_model(locations, labels);
                    net->get_parameters();
                    test_errors(all_points, net, max_error, min_error);
                    cout << "max_error: " << max_error << endl;
                    cout << "min_error: " << min_error << endl;
                    finish = chrono::high_resolution_clock::now();
                    build_time = read_data_time + chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
                }
                if (method != "rl")
                {
#ifdef use_gpu
                    net->to(torch::kCUDA);
#endif
                    torch::load(net, model_path);
                    net->get_parameters();
                }
                start = chrono::high_resolution_clock::now();

                test_point_query(all_points, net, max_error, min_error);
                finish = chrono::high_resolution_clock::now();
                double query_time = chrono::duration_cast<chrono::nanoseconds>(finish - start).count() * 1.0 / N;
                build_time = build_time * 1.0 / 1000000000;
                cout << "query_time: " << query_time << endl;
                file_writer.write_cost_model_data(N, distribution, method, build_time, query_time);
                // break;
            }
        }
    }

    // std::shared_ptr<Net>
    auto query_cost_model = std::make_shared<Net>(10, 32);
    auto build_cost_model = std::make_shared<Net>(10, 32);
    void cost_model_build()
    {
        cout << "cost_model_build" << endl;
        FileReader filereader(",");
        string path = "/home/cwang39/RSMI/cost_model/train_set_formatted.csv";
        // string path = "/home/liuguanli/Dropbox/shared/VLDB20/codes/rsmi/cost_model/train_set_formatted_normalized.csv";
        string build_time_model_path = "/home/cwang39/RSMI/cost_model/build_time_model_rsmi.pt";
        string query_time_model_path = "/home/cwang39/RSMI/cost_model/query_time_model_rsmi.pt";

        std::ifstream fin_build(build_time_model_path);
        std::ifstream fin_query(query_time_model_path);
#ifdef use_gpu
        query_cost_model->to(torch::kCUDA);
        build_cost_model->to(torch::kCUDA);
#endif
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
// #ifdef use_gpu
//             query_cost_model->to(torch::kCUDA);
//             build_cost_model->to(torch::kCUDA);
// #endif
            build_cost_model->train_model(parameters, build_time_labels);
            query_cost_model->train_model(parameters, query_time_labels);
            torch::save(build_cost_model, build_time_model_path);
            torch::save(query_cost_model, query_time_model_path);
        }
    }

    // TODO use test file and one real case to test it.
    // first use all data from

    // use osm as a test case.
    void evaluate_cost_model(float lambda)
    {
        cout << "evaluate_cost_model " << endl;
        // read file
        string path = "/home/cwang39/RSMI/cost_model/train_set_formatted_test.csv";
        FileReader filereader(",");
        vector<float> parameters;
        vector<float> build_time_labels;
        vector<float> query_time_labels;
        vector<float> distribution = {0, 0, 1};
        float cardinality = 1.0 / 6400;
        filereader.get_cost_model_data(path, parameters, build_time_labels, query_time_labels);
        int max_exp_index;
        int max_pred_index;

        float max_exp_score;
        float max_pred_score;
        int n = build_time_labels.size();

        int correct_num = 0;
        int total_num = 0;
        for (size_t i = 0; i < n; i++)
        {
            std::vector<float> temp(parameters.begin() + i * 10, parameters.begin() + i * 10 + 10);
            std::vector<float> temp_distribution(parameters.begin() + i * 10 + 1, parameters.begin() + i * 10 + 4);
#ifdef use_gpu
            torch::Tensor x = torch::tensor(temp, at::kCUDA).reshape({1, 10});
#else
            torch::Tensor x = torch::tensor(temp).reshape({1, 10});
#endif
            // cout << "temp: " << temp << endl;
            float build_score = build_cost_model->predict(x).item().toFloat();
            float query_score = query_cost_model->predict(x).item().toFloat();
            float score = lambda * build_score + (1 - lambda) * query_score;
            float expected_score = lambda * build_time_labels[i] + (1 - lambda) * query_time_labels[i];
            // cout << "cardinality: " << temp[0] << endl;
            // cout << "expected score: " << expected_score << "\treal score: " << score << endl;
            if (cardinality == temp[0] && distribution == temp_distribution)
            {
                if (max_exp_score < expected_score)
                {
                    max_exp_index = i;
                }

                if (max_pred_score < score)
                {
                    max_pred_index = i;
                }
            }
            else
            {
                cardinality = temp[0];
                distribution = temp_distribution;
                total_num++;
                if (max_pred_index == max_exp_index)
                {
                    correct_num++;
                }
                max_exp_score = expected_score;
                max_pred_score = score;
            }
        }
        cout << "lambda: " << lambda << endl;
        cout << "accuracy: " << correct_num * 100.0 / total_num << "%" << endl;
        // prepare osm train time and query time according to the result in experiment
    }

    string get_distribution(Histogram hist, string type)
    {
        Histogram uniform = type == "Z" ? Net::pre_trained_histograms_rsmi_Z["uniform_1000_1_2_"] : Net::pre_trained_histograms_rsmi_H["uniform_1000_1_2_"];
        Histogram normal = type == "Z" ? Net::pre_trained_histograms_rsmi_Z["uniform_1000_1_2_"] : Net::pre_trained_histograms_rsmi_H["normal_1000_1_2_0.500000_0.125000_"];
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

    void cost_model_predict(ExpRecorder &exp_recorder, float lambda, float cardinality, string distribution)
    {
        // cout << "cardinality: " << cardinality << endl;
        // cout << "distribution: " << distribution << endl;
        map<string, vector<float>> distributions = {
            {"normal", {1, 0, 0}}, {"skewed", {0, 1, 0}}, {"uniform", {0, 0, 1}}};

        vector<float> distribution_list = distributions[distribution];
        // {"cl", {1, 0, 0, 0, 0, 0}},
        // mr = 1
        // or = 2
        // rl = 3
        // rs = 4
        // sp = 5
        map<int, vector<float>> methods = {
            {1, {0, 1, 0, 0, 0, 0}}, {2, {0, 0, 1, 0, 0, 0}}, {3, {0, 0, 0, 1, 0, 0}}, {4, {0, 0, 0, 0, 1, 0}}, {5, {0, 0, 0, 0, 0, 1}}};

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
            if (score > max_score)
            {
                max_score = score;
                result = iter->first;
            }
            // cout << "iter->first: " << iter->first << " build_score: " << build_score << " query_score: " << query_score << " score: " << score << endl;
            iter++;
        }
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