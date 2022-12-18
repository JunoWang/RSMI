#include <iostream>
#include <vector>
#include <queue>
#include "../entities/Node.h"
#include "../entities/Point.h"
#include "../entities/Mbr.h"
#include "../entities/NonLeafNode.h"
#include <typeinfo>
// #include "../file_utils/SortTools.h"
// #include "../file_utils/ModelTools.h"
#include "../curves/hilbert.H"
#include "../curves/hilbert4.H"
#include <map>
#include <boost/smart_ptr/make_shared_object.hpp>
#include <torch/script.h>
#include <ATen/ATen.h>
#include <torch/torch.h>

#include <torch/optim.h>
#include <torch/types.h>
#include <torch/utils.h>

#include "../utils/PreTrainRSMI.h"
#include "../utils/Rebuild.h"
#include "../utils/SortTools.h"

using namespace at;
using namespace torch::nn;
using namespace torch::optim;
using namespace std;

class RSMI
{

private:
    int level;
    int index;
    int max_partition_num;
    long long N = 0;
    int max_error = 0;
    int min_error = 0;
    int width = 0;
    int leaf_node_num;

    int bit_num;

    bool is_last;
    Mbr mbr;
    std::shared_ptr<Net> net;

    float x_gap = 1.0;
    float x_scale = 1.0;
    float x_0 = 0;
    float x_1 = 0;
    float y_gap = 1.0;
    float y_scale = 1.0;
    float y_0 = 0;
    float y_1 = 0;
    bool is_reused = false;
    long long side = 0;

public:
    string model_path;
    static string model_path_root;
    map<int, RSMI> children;
    vector<LeafNode> leafnodes;
    vector<float> points_x;
    vector<float> points_y;
    float sampling_rate = 1.0;

    double num_rec_before_refine_leaf;
    double num_rec_before_refine_cum = 0;

    RSMI();
    RSMI(int index, int max_partition_num);
    RSMI(int index, int level, int max_partition_num);
    void build(ExpRecorder &exp_recorder, vector<Point> points);
    void rebuild(ExpRecorder &exp_recorder, vector<Point> points);
    void print_index_info(ExpRecorder &exp_recorder);

    bool point_query(ExpRecorder &exp_recorder, Point query_point);
    void point_query(ExpRecorder &exp_recorder, vector<Point> &query_points);
    int predict(ExpRecorder &exp_recorder, Point query_point, int width);

    void window_query(ExpRecorder &exp_recorder, vector<Mbr> query_windows);
    // vector<Point> window_query(ExpRecorder &exp_recorder, Mbr query_window);
    void window_query(ExpRecorder &exp_recorder, vector<Point> vertexes, Mbr query_window, float boundary, int k, Point query_point, float &);
    void window_query(ExpRecorder &exp_recorder, vector<Point> vertexes, Mbr query_window);
    void acc_window_query(ExpRecorder &exp_recorder, vector<Mbr> query_windows);
    vector<Point> acc_window_query(ExpRecorder &exp_recorder, Mbr query_windows);

    void kNN_query(ExpRecorder &exp_recorder, vector<Point> query_points, int k);
    // vector<Point> kNN_query(ExpRecorder &exp_recorder, Point query_point, int k);
    void kNN_query(ExpRecorder &exp_recorder, Point query_point, int k);
    void acc_kNN_query(ExpRecorder &exp_recorder, vector<Point> query_points, int k);
    vector<Point> acc_kNN_query(ExpRecorder &exp_recorder, Point query_point, int k);
    double cal_rho(Point point);
    double knn_diff(vector<Point> acc, vector<Point> pred);

    void insert(ExpRecorder &exp_recorder, Point);
    void insert(ExpRecorder &exp_recorder);
    void insert(ExpRecorder &exp_recorder, int index);
    bool insert(ExpRecorder &exp_recorder, vector<Point> &);

    void remove(ExpRecorder &exp_recorder, Point);
    void remove(ExpRecorder &exp_recorder, vector<Point>);

    int binary_search(vector<float> &, float);
    int binary_search_x(float);
    int binary_search_y(float);

    void traverse(ExpRecorder &exp_recorder);
    void clear(ExpRecorder &exp_recorder);
};

RSMI::RSMI()
{
    // leafnodes = vector<LeafNode>(10);
}

RSMI::RSMI(int index, int max_partition_num)
{
    this->index = index;
    this->max_partition_num = max_partition_num;
    this->level = 0;
}

RSMI::RSMI(int index, int level, int max_partition_num)
{
    this->index = index;
    this->level = level;
    this->max_partition_num = max_partition_num;
}

void RSMI::build(ExpRecorder &exp_recorder, vector<Point> points)
{
    // cout << "build" << endl;
    int page_size = Constants::PAGESIZE;
    auto start = chrono::high_resolution_clock::now();
    std::stringstream stream;
    stream << std::fixed << std::setprecision(1) << exp_recorder.model_reuse_threshold;
    string threshold = stream.str();
    bit_num = ceil((log(N / Constants::RESOLUTION)) / log(2)) * 2;
    // if (points.size() <= 256 * 100)
    if (points.size() <= exp_recorder.N)
    {
        this->model_path += "_" + to_string(level) + "_" + to_string(index);
        if (exp_recorder.depth < level)
        {
            exp_recorder.depth = level;
        }
        exp_recorder.last_level_model_num++;
        is_last = true;
        N = points.size();
        side = pow(2, ceil(log(N) / log(2)));
        sort(points.begin(), points.end(), sortX());
        x_gap = 1.0 / (points[N - 1].x - points[0].x);
        x_scale = 1.0 / x_gap;
        x_0 = points[0].x;
        x_1 = points[N - 1].x;
        for (int i = 0; i < N; i++)
        {
            points[i].x_i = i;
            points_x.push_back(points[i].x);
            mbr.update(points[i].x, points[i].y);
        }
        sort(points.begin(), points.end(), sortY());
        y_gap = points[N - 1].y - points[0].y;
        y_scale = 1.0 / y_gap;
        y_0 = points[0].y;
        y_1 = points[N - 1].y;
        for (int i = 0; i < N; i++)
        {
            points[i].y_i = i;
            points_y.push_back(points[i].y);
            auto start_sfc = chrono::high_resolution_clock::now();
            // long long xs[2] = {(long long)points[i].x_i, points[i].y_i};
            // long long curve_val = compute_Hilbert_value(xs, 2, side);
            points[i].curve_val = compute_Hilbert_value(points[i].x_i, points[i].y_i, side);
            auto finish_sfc = chrono::high_resolution_clock::now();
            exp_recorder.sfc_cal_time += chrono::duration_cast<chrono::nanoseconds>(finish_sfc - start_sfc).count();
        }
        sort(points.begin(), points.end(), sort_curve_val());
        long long h_min = points[0].curve_val;
        long long h_max = points[N - 1].curve_val;
        long long h_gap = h_max - h_min + 1;

        if (exp_recorder.is_cost_model)
        {
            vector<float> locations_;
            for (Point point : points)
            {
                locations_.push_back((point.curve_val - h_min) * 1.0 / h_gap);
            }
            Histogram histogram(pow(2, Constants::UNIFIED_Z_BIT_NUM), locations_);
            pre_train_rsmi::cost_model_predict(exp_recorder, exp_recorder.lower_level_lambda, locations_.size() * 1.0 / 10000, pre_train_rsmi::get_distribution(histogram, "H"));
        }

        width = N - 1;
        if (N == 1)
        {
            points[0].index = 0;
        }
        else
        {
            for (long i = 0; i < N; i++)
            {
                points[i].index = i * 1.0 / (N - 1);
            }
        }
        leaf_node_num = points.size() / page_size;
        for (int i = 0; i < leaf_node_num; i++)
        {
            LeafNode leafNode;
            auto bn = points.begin() + i * page_size;
            auto en = points.begin() + i * page_size + page_size;
            vector<Point> vec(bn, en);
            // cout << vec.size() << " " << vec[0]->x_i << " " << vec[99]->x_i << endl;
            leafNode.add_points(vec);
            leafnodes.push_back(leafNode);
        }

        // for the last leafNode
        if (points.size() > page_size * leaf_node_num)
        {
            LeafNode leafNode;
            auto bn = points.begin() + page_size * leaf_node_num;
            auto en = points.end();
            vector<Point> vec(bn, en);
            leafNode.add_points(vec);
            leafnodes.push_back(leafNode);
            leaf_node_num++;
        }
        exp_recorder.leaf_node_num += leaf_node_num;
        if (exp_recorder.is_model_reuse)
        {
            net = std::make_shared<Net>(2);
        }
        else
        {
            // net = std::make_shared<Net>(2, leaf_node_num / 2 + 2);
            net = std::make_shared<Net>(2, Constants::HIDDEN_LAYER_WIDTH);
        }

#ifdef use_gpu
        net->to(torch::kCUDA);
#endif
        vector<float> locations;
        vector<float> labels;
        if (exp_recorder.is_model_reuse)
        {
            // cout << "exp_recorder.is_mr" << endl;
            exp_recorder.model_reuse_num++;
            vector<float> features;
            auto start_mr = chrono::high_resolution_clock::now();
            for (Point point : points)
            {
                locations.push_back(point.x);
                locations.push_back(point.y);
                features.push_back((point.curve_val - h_min) / h_gap);
                labels.push_back(point.index);
            }
            Histogram histogram(pow(2, Constants::UNIFIED_Z_BIT_NUM), features);
            if (net->is_reusable_rsmi_H(histogram, threshold, this->model_path))
            {
                is_reused = true;
                // cout << "leaf model_path: " << model_path << endl;
                torch::load(net, this->model_path);
                auto finish_mr = chrono::high_resolution_clock::now();
                exp_recorder.extra_time += chrono::duration_cast<chrono::nanoseconds>(finish_mr - start_mr).count();
            }
            else
            {
                net->train_model(locations, labels);
            }
            // cout << "exp_recorder.is_mr finish" << endl;
        }
        else if (exp_recorder.is_sp)
        {
            // cout << "exp_recorder.is_sp" << endl;
            exp_recorder.sp_num++;
            auto start_sp = chrono::high_resolution_clock::now();
            int sample_gap = 1 / sampling_rate;
            long long counter = 0;
            for (Point point : points)
            {
                if (counter % sample_gap == 0)
                {
                    locations.push_back(point.x);
                    locations.push_back(point.y);
                    labels.push_back(point.index);
                }
                counter++;
            }
            if (sample_gap >= N)
            {
                Point last_point = points[N - 1];
                locations.push_back(last_point.x);
                locations.push_back(last_point.y);
                labels.push_back(last_point.index);
            }
            auto finish_sp = chrono::high_resolution_clock::now();
            exp_recorder.extra_time += chrono::duration_cast<chrono::nanoseconds>(finish_sp - start_sp).count();
            net->train_model(locations, labels);
            // cout << "exp_recorder.is_sp finish" << endl;
        }
        else if (exp_recorder.is_rs)
        {
            exp_recorder.rs_num++;
            if (N < 10 * sqrt(exp_recorder.rs_threshold_m))
            {
                // cout << "exp_recorder.is_rs 1" << endl;
                auto start_train = chrono::high_resolution_clock::now();
                net->train_model(locations, labels);
                auto end_train = chrono::high_resolution_clock::now();
                exp_recorder.training_cost += chrono::duration_cast<chrono::nanoseconds>(end_train - start_train).count();
                // cout << "exp_recorder.is_rs 1 finish" << endl;
            }
            else
            {
                // cout << "exp_recorder.is_rs 2" << endl;
                auto start_rs = chrono::high_resolution_clock::now();
                vector<Point> rs_points = pre_train_zm::get_rep_set_space(exp_recorder.rs_threshold_m / 10, x_0, y_0, x_gap / 2, y_gap / 2, points);
                int temp_N = rs_points.size();
                // cout << "temp_N: " << temp_N << endl;
                vector<float> rs_locations;
                vector<float> rs_labels;
                for (Point point : rs_points)
                {
                    rs_locations.push_back(point.x);
                    rs_locations.push_back(point.y);
                    rs_labels.push_back(point.index);
                }
                auto finish_rs = chrono::high_resolution_clock::now();
                exp_recorder.extra_time += chrono::duration_cast<chrono::nanoseconds>(finish_rs - start_rs).count();
                auto start_train = chrono::high_resolution_clock::now();
                net->train_model(rs_locations, rs_labels);
                auto end_train = chrono::high_resolution_clock::now();
                exp_recorder.training_cost += chrono::duration_cast<chrono::nanoseconds>(end_train - start_train).count();
                // cout << "exp_recorder.is_rs 2 finish" << endl;
            }
        }
        // else if (exp_recorder.is_rl)
        // {
        //     exp_recorder.rl_num++;
        //     // cout << "exp_recorder.is_rl" << endl;
        //     auto start_rl = chrono::high_resolution_clock::now();
        //     int bit_num = exp_recorder.bit_num;
        //     // pre_train_zm::write_approximate_SFC(Constants::DATASETS, exp_recorder.distribution + "_" + to_string(exp_recorder.dataset_cardinality) + "_" + to_string(exp_recorder.skewness) + "_2_.csv", bit_num);
        //     pre_train_zm::write_approximate_SFC(points, exp_recorder.get_file_name(), bit_num);
        //     string commandStr = "python /home/liuguanli/Documents/pre_train/rl_4_sfc/RL_4_SFC_RSMI.py -d " +
        //                         exp_recorder.distribution + " -s " + to_string(exp_recorder.dataset_cardinality) + " -n " +
        //                         to_string(exp_recorder.skewness) + " -m 2 -b " + to_string(bit_num) +
        //                         " -f /home/liuguanli/Documents/pre_train/sfc_z_weight/bit_num_%d/%s_%d_%d_%d_.csv";
        //     char command[1024];
        //     strcpy(command, commandStr.c_str());
        //     int res = system(command);

        //     vector<int> sfc;
        //     vector<float> cdf;
        //     FileReader RL_SFC_reader("", ",");
        //     int bit_num_shrinked = 6;
        //     vector<float> features;
        //     RL_SFC_reader.read_sfc_2d("/home/liuguanli/Documents/pre_train/sfc_z/" + to_string(bit_num_shrinked) + "_" + exp_recorder.distribution + "_" + to_string(exp_recorder.dataset_cardinality) + "_" + to_string(exp_recorder.skewness) + "_2_.csv", features, cdf);
        //     // cout << "features.size(): " << features.size() << endl;
        //     // cout << "cdf.size(): " << cdf.size() << endl;
        //     auto finish_rl = chrono::high_resolution_clock::now();
        //     exp_recorder.top_rl_time = chrono::duration_cast<chrono::nanoseconds>(finish_rl - start_rl).count();
        //     exp_recorder.extra_time += exp_recorder.top_rl_time;
        //     net->train_model(features, cdf);
        //     // cout << "exp_recorder.is_rl finish" << endl;
        //     // torch::save(net, this->model_path);
        //     // cout << "RL_SFC finish" << endl;
        // }
        // else if (exp_recorder.is_cluster)
        // {
        //     auto start_rs = chrono::high_resolution_clock::now();

        //     auto finish_rs = chrono::high_resolution_clock::now();
        //     exp_recorder.extra_time += chrono::duration_cast<chrono::nanoseconds>(finish_rs - start_rs).count();
        // }
        else
        {
            // cout << "exp_recorder. is og" << endl;
            exp_recorder.original_num++;
            for (Point point : points)
            {
                locations.push_back(point.x);
                locations.push_back(point.y);
                labels.push_back(point.index);
            }
            net->train_model(locations, labels);
            // cout << "exp_recorder. is og finish" << endl;
        }
        net->get_parameters();
        exp_recorder.non_leaf_node_num++;
        int total_errors = 0;
        for (int i = 0; i < N; i++)
        {
            Point point = points[i];
            int predicted_index;
            if (is_reused)
            {
                float x1 = (point.x - x_0) * x_scale + x_0;
                float x2 = (point.y - y_0) * y_scale + y_0;
                predicted_index = (int)(net->predict(x1, x2) * leaf_node_num);
                // predicted_index = (int)(net->predict(point, x_scale, y_scale, x_0, y_0) * leaf_node_num);
            }
            else
            {
                predicted_index = (int)(net->predict(point) * leaf_node_num);
            }

            predicted_index = predicted_index < 0 ? 0 : predicted_index;
            predicted_index = predicted_index >= leaf_node_num ? leaf_node_num - 1 : predicted_index;

            int error = i / page_size - predicted_index;
            total_errors += abs(error);
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
        exp_recorder.bottom_error += total_errors;
        if ((max_error - min_error) > (exp_recorder.max_error - exp_recorder.min_error))
        {
            exp_recorder.max_error = max_error;
            exp_recorder.min_error = min_error;
        }
        auto finish = chrono::high_resolution_clock::now();
        exp_recorder.bottom_level_time += chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
    }
    else
    {
        exp_recorder.non_leaf_model_num++;
        is_last = false;
        N = (long long)points.size();
        bit_num = max_partition_num;
        int partition_size = ceil(points.size() * 1.0 / pow(bit_num, 2));
        sort(points.begin(), points.end(), sortY());
        y_gap = points[N - 1].y - points[0].y;
        y_scale = 1.0 / y_gap;
        y_0 = points[0].y;
        y_1 = points[N - 1].y;
        sort(points.begin(), points.end(), sortX());
        x_gap = points[N - 1].x - points[0].x;
        x_scale = 1.0 / x_gap;
        x_0 = points[0].x;
        x_1 = points[N - 1].x;
        long long side = pow(bit_num, 2);
        width = side - 1;
        map<int, vector<Point>> points_map;
        int each_item_size = partition_size * bit_num;
        long long point_index = 0;

        vector<float> locations(N * 2);
        vector<float> labels(N);
        // vector<long long> features;
        long long xs_min[2] = {0, 0};
        long long z_min = compute_Z_value(xs_min, 2, side);
        long long xs_max[2] = {bit_num - 1, bit_num - 1};
        long long z_max = compute_Z_value(xs_max, 2, side);
        long long z_gap = z_max - z_min;
        for (size_t i = 0; i < bit_num; i++)
        {
            long long bn_index = i * each_item_size;
            long long end_index = bn_index + each_item_size;
            if (bn_index >= N)
            {
                break;
            }
            else
            {
                if (end_index > N)
                {
                    end_index = N;
                }
            }
            auto bn = points.begin() + bn_index;
            auto en = points.begin() + end_index;
            vector<Point> vec(bn, en);
            sort(vec.begin(), vec.end(), sortY());
            for (size_t j = 0; j < bit_num; j++)
            {
                long long sub_bn_index = j * partition_size;
                long long sub_end_index = sub_bn_index + partition_size;
                if (sub_bn_index >= vec.size())
                {
                    break;
                }
                else
                {
                    if (sub_end_index > vec.size())
                    {
                        sub_end_index = vec.size();
                    }
                }
                auto sub_bn = vec.begin() + sub_bn_index;
                auto sub_en = vec.begin() + sub_end_index;
                vector<Point> sub_vec(sub_bn, sub_en);
                long long xs[2] = {i, j};
                auto start_sfc = chrono::high_resolution_clock::now();
                long long Z_value = compute_Z_value(xs, 2, side);
                auto finish_sfc = chrono::high_resolution_clock::now();
                exp_recorder.sfc_cal_time += chrono::duration_cast<chrono::nanoseconds>(finish_sfc - start_sfc).count();
                // cout << "Z_value: " << Z_value << endl;
                int sub_point_index = 1;
                long sub_size = sub_vec.size();
                int counter = 0;
                for (Point point : sub_vec)
                {
                    point.index = (Z_value - z_min) * 1.0 / z_gap;
                    locations[point_index * 2] = point.x;
                    locations[point_index * 2 + 1] = point.y;
                    labels[point_index] = point.index;
                    // features.push_back(point.index);
                    points[point_index].index = point.index;
                    point_index++;
                    mbr.update(point.x, point.y);
                    sub_point_index++;
                }
                // vector<Point> temp;
                // points_map.insert(pair<int, vector<Point>>(Z_value, temp));
            }
        }

        if (exp_recorder.is_cost_model)
        {
            Histogram histogram(pow(2, Constants::UNIFIED_Z_BIT_NUM), labels);
            if (level == 0)
            {
                exp_recorder.ogiginal_histogram = histogram;
                exp_recorder.changing_histogram = histogram;
            }
            pre_train_rsmi::cost_model_predict(exp_recorder, exp_recorder.upper_level_lambda, labels.size() * 1.0 / 10000, pre_train_rsmi::get_distribution(histogram, "H"));
        }

        int epoch = Constants::START_EPOCH;
        bool is_retrain = false;
        do
        {
            long long total_errors = 0;
            net = std::make_shared<Net>(2);
#ifdef use_gpu
            net->to(torch::kCUDA);
#endif
            if (!is_retrain)
            {
                this->model_path += "_" + to_string(level) + "_" + to_string(index);
            }
            if (exp_recorder.is_rl)
            {
                // cout << "exp_recorder.is_rl nonleaf" << endl;
                exp_recorder.rl_num++;
                // cout << "RL_SFC begin" << endl;
                auto start_rl = chrono::high_resolution_clock::now();
                int bit_num = exp_recorder.bit_num;
                // pre_train_zm::write_approximate_SFC(Constants::DATASETS, exp_recorder.distribution + "_" + to_string(exp_recorder.dataset_cardinality) + "_" + to_string(exp_recorder.skewness) + "_2_.csv", bit_num);
                pre_train_zm::write_approximate_SFC(points, exp_recorder.get_file_name(), bit_num);
                string commandStr = "python /home/liuguanli/Documents/pre_train/rl_4_sfc/RL_4_SFC_RSMI.py -d " +
                                    exp_recorder.distribution + " -s " + to_string(exp_recorder.dataset_cardinality) + " -n " +
                                    to_string(exp_recorder.skewness) + " -m 2 -b " + to_string(bit_num) +
                                    " -f /home/liuguanli/Documents/pre_train/sfc_z_weight/bit_num_%d/%s_%d_%d_%d_.csv";
                char command[1024];
                strcpy(command, commandStr.c_str());
                int res = system(command);

                vector<int> sfc;
                vector<float> cdf;
                FileReader RL_SFC_reader("", ",");
                int bit_num_shrinked = 6;
                vector<float> features;
                RL_SFC_reader.read_sfc_2d("/home/liuguanli/Documents/pre_train/sfc_z/" + to_string(bit_num_shrinked) + "_" + exp_recorder.distribution + "_" + to_string(exp_recorder.dataset_cardinality) + "_" + to_string(exp_recorder.skewness) + "_2_.csv", features, cdf);
                // cout << "features.size(): " << features.size() << endl;
                // cout << "cdf.size(): " << cdf.size() << endl;
                auto finish_rl = chrono::high_resolution_clock::now();
                exp_recorder.top_rl_time = chrono::duration_cast<chrono::nanoseconds>(finish_rl - start_rl).count();
                exp_recorder.extra_time += exp_recorder.top_rl_time;
                net->train_model(features, cdf);
                // torch::save(net, this->model_path);
                // cout << "RL_SFC finish" << endl;
                // cout << "exp_recorder.is_rl nonleaf finish" << endl;
            }
            else if (exp_recorder.is_rs)
            {
                // cout << "exp_recorder.is_rs nonleaf" << endl;
                exp_recorder.rs_num++;
                // cout << "is_rs" << endl;
                auto start_rs = chrono::high_resolution_clock::now();
                // 142 same points!!!
                int m = N > 10 * exp_recorder.rs_threshold_m ? exp_recorder.rs_threshold_m : sqrt(exp_recorder.rs_threshold_m) * 10;
                // cout << "m: " << m << " x_0: " << x_0 << " y_0: " << y_0 << " x_gap: " << x_gap << " y_gap: " << y_gap << endl;
                // cout << "points.size: " << points.size() << endl;
                vector<Point> rs_points = pre_train_zm::get_rep_set_space(m, x_0, y_0, x_gap / 2, y_gap / 2, points);
                int temp_N = rs_points.size();
                // cout << "temp_N: " << temp_N << endl;
                vector<float> rs_locations;
                vector<float> rs_labels;
                for (Point point : rs_points)
                {
                    rs_locations.push_back(point.x);
                    rs_locations.push_back(point.y);
                    rs_labels.push_back(point.index);
                }
                // cout << "rs_labels: " << rs_labels << endl;
                auto finish_rs = chrono::high_resolution_clock::now();
                exp_recorder.extra_time += chrono::duration_cast<chrono::nanoseconds>(finish_rs - start_rs).count();
                auto start_train = chrono::high_resolution_clock::now();
                net->train_model(rs_locations, rs_labels);
                auto end_train = chrono::high_resolution_clock::now();
                exp_recorder.training_cost += chrono::duration_cast<chrono::nanoseconds>(end_train - start_train).count();
                exp_recorder.extra_time += chrono::duration_cast<chrono::nanoseconds>(finish_rs - start_rs).count();
                // cout << "exp_recorder.is_rs nonleaf finish" << endl;
            }
            else if (exp_recorder.is_sp)
            {
                // cout << "exp_recorder.is_sp nonleaf" << endl;
                exp_recorder.sp_num++;
                // cout << "is_sp" << endl;
                auto start_sp = chrono::high_resolution_clock::now();
                int sample_gap = 1 / sampling_rate;
                long long counter = 0;
                vector<float> locations_sp;
                vector<float> labels_sp;
                for (size_t i = 0; i < labels.size(); i += sample_gap)
                {
                    locations_sp.push_back(locations[2 * i]);
                    locations_sp.push_back(locations[2 * i + 1]);
                    labels_sp.push_back(labels[i]);
                }
                // cout << "labels_sp size: " << labels_sp.size() << " sampling_rate: " << sampling_rate << endl;
                auto finish_sp = chrono::high_resolution_clock::now();
                exp_recorder.extra_time += chrono::duration_cast<chrono::nanoseconds>(finish_sp - start_sp).count();
                net->train_model(locations_sp, labels_sp);
                // cout << "exp_recorder.is_sp nonleaf finish" << endl;
            }
            else if (exp_recorder.is_model_reuse)
            {
                // cout << "exp_recorder.is_mr non-leaf " << endl;
                exp_recorder.model_reuse_num++;
                // cout << "is_model_reuse" << endl;
                // SFC sfc(bit_num, features);
                // sfc.gen_CDF(Constants::UNIFIED_Z_BIT_NUM);
                auto start_mr = chrono::high_resolution_clock::now();
                Histogram histogram(pow(2, Constants::UNIFIED_Z_BIT_NUM), labels);
                if (net->is_reusable_rsmi_Z(histogram, threshold, this->model_path) && !is_retrain)
                {
                    is_reused = true;
                    // cout << "nonleaf model_path: " << model_path << endl;
                    torch::load(net, this->model_path);
                    auto finish_mr = chrono::high_resolution_clock::now();
                    exp_recorder.extra_time += chrono::duration_cast<chrono::nanoseconds>(finish_mr - start_mr).count();
                    // net->get_parameters();
                    // cout<< "nonleaf load finish: " << endl;
                }
                else
                {
                    exp_recorder.original_num++;
                    net->train_model(locations, labels);
                    // net->get_parameters();
                    torch::save(net, this->model_path);
                }
                // cout << "exp_recorder.is_mr non-leaf finish" << endl;
            }
            else
            {
                // cout << "exp_recorder.is_og" << endl;
                exp_recorder.original_num++;
                net->train_model(locations, labels);
                // cout << "exp_recorder.is_og finish" << endl;
            }
            net->get_parameters();

            for (Point point : points)
            {
                int predicted_index;
                if (is_reused && !is_retrain)
                {
                    float x1 = (point.x - x_0) * x_scale + x_0;
                    float x2 = (point.y - y_0) * y_scale + y_0;
                    predicted_index = (int)(net->predict(x1, x2) * width);
                }
                else
                {
                    predicted_index = (int)(net->predict(point) * width);
                }
                // cout<< "net->predict(point): " << net->predict(point) << endl;
                // cout<< "width: " << width << endl;
                // cout<< "predicted_index: " << predicted_index << endl;
                predicted_index = predicted_index < 0 ? 0 : predicted_index;
                predicted_index = predicted_index >= width ? width - 1 : predicted_index;
                points_map[predicted_index].push_back(point);

                if (level == 0)
                {
                    total_errors += abs((int)(predicted_index - point.index * width));
                }
            }
            if (level == 0)
            {
                exp_recorder.top_error = total_errors / points.size();
                exp_recorder.loss = net->total_loss;
            }
            map<int, vector<Point>>::iterator iter1;
            iter1 = points_map.begin();
            int map_size = 0;
            while (iter1 != points_map.end())
            {
                if (iter1->second.size() > 0)
                {
                    map_size++;
                }
                // cout<< "iter1->first: " << iter1->first << endl;
                iter1++;
            }
            // if (map_size == 1)
            // {
            //     iter1--;
            //     cout<< "iter1->first: " << iter1->first << endl;
            // }

            // cout << "map_size: " << map_size << endl;
            // cout << "width: " << width << endl;
            // cout << "x_gap: " << x_gap << endl;
            // cout << "y_gap: " << y_gap << endl;
            // cout << "N: " << N << endl;
            if (map_size < 2)
            {
                int predicted_index;
                if (is_reused)
                {
                    float x1 = (points[0].x - x_0) * x_gap + x_0;
                    float x2 = (points[0].y - y_0) * y_gap + y_0;
                    predicted_index = (int)(net->predict(x1, x2) * width);
                }
                else
                {
                    predicted_index = (int)(net->predict(points[0]) * width);
                }
                // cout<< "net->predict(points[0]): " << net->predict(points[0]) << endl;
                // cout << "need rebuild !!!!!! predicted_index: " << predicted_index << endl;
                predicted_index = predicted_index < 0 ? 0 : predicted_index;
                predicted_index = predicted_index >= width ? width - 1 : predicted_index;
                points_map[predicted_index].clear();
                points_map[predicted_index].shrink_to_fit();
                is_retrain = true;
                epoch = Constants::EPOCH_ADDED;
            }
            else
            {
                is_retrain = false;
            }

        } while (is_retrain);
        auto finish = chrono::high_resolution_clock::now();
        if (level == 0)
        {
            exp_recorder.top_level_time = chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
        }

        exp_recorder.non_leaf_node_num++;

        points.clear();
        points.shrink_to_fit();

        map<int, vector<Point>>::iterator iter;
        iter = points_map.begin();

        while (iter != points_map.end())
        {
            if (iter->second.size() > 0)
            {
                RSMI partition(iter->first, level + 1, max_partition_num);
                partition.model_path = model_path;
                partition.sampling_rate = 0.01;
                partition.build(exp_recorder, iter->second);
                iter->second.clear();
                iter->second.shrink_to_fit();
                children.insert(pair<int, RSMI>(iter->first, partition));
            }
            iter++;
        }
    }
}

void RSMI::print_index_info(ExpRecorder &exp_recorder)
{
    cout << "finish point_query max_error: " << exp_recorder.max_error << endl;
    cout << "finish point_query min_error: " << exp_recorder.min_error << endl;
    cout << "finish point_query top_error: " << exp_recorder.top_error << endl;
    cout << "finish point_query bottom_error: " << exp_recorder.bottom_error << endl;
    cout << "last_level_model_num: " << exp_recorder.last_level_model_num << endl;
    cout << "leaf_node_num: " << exp_recorder.leaf_node_num << endl;
    cout << "non_leaf_node_num: " << exp_recorder.non_leaf_node_num << endl;
    cout << "depth: " << exp_recorder.depth << endl;
}

int RSMI::binary_search(vector<float> &coordinates, float target)
{
    int front = 0;
    int back = coordinates.size() - 1;
    while (front <= back)
    {
        int mid = (front + back) / 2;
        float temp = coordinates[mid];
        if (temp > target)
        {
            back = mid - 1;
        }
        else if (temp < target)
        {
            front = mid + 1;
        }
        else
        {
            return mid;
        }
    }
    return -1;
}

// int RSMI::binary_search_x(float target)
// {
//     int front = 0;
//     int back = points_x.size() - 1;
//     while (front <= back)
//     {
//         int mid = (front + back) / 2;
//         float temp = points_x[mid];
//         if (temp > target)
//         {
//             back = mid - 1;
//         }
//         else if (temp < target)
//         {
//             front = mid + 1;
//         }
//         else
//         {
//             return mid;
//         }
//     }
//     return -1;
// }

// int RSMI::binary_search_y(float target)
// {
//     int front = 0;
//     int back = points_y.size() - 1;
//     while (front <= back)
//     {
//         int mid = (front + back) / 2;
//         float temp = points_y[mid];
//         if (temp > target)
//         {
//             back = mid - 1;
//         }
//         else if (temp < target)
//         {
//             front = mid + 1;
//         }
//         else
//         {
//             return mid;
//         }
//     }
//     return -1;
// }

bool RSMI::point_query(ExpRecorder &exp_recorder, Point query_point)
{
    exp_recorder.search_steps++;
    if (is_last)
    {
        int predicted_index = 0;
        // predicted_index = net->predict(query_point) * leaf_node_num;
        // auto start_sfc = chrono::high_resolution_clock::now();
        // int x_index = binary_search_x(query_point.x);
        // int y_index = binary_search_y(query_point.y);
        // if (x_index == -1 || y_index == -1)
        // {
        //     exp_recorder.point_not_found++;
        //     return false;
        // }
        // long long curve_val = compute_Hilbert_value(x_index, y_index, side);
        // // cout << "x_index: " << x_index << "y_index: " << y_index << " curve_val: " << curve_val << " side: " << side << " N:" << N << endl;
        // auto finish_sfc = chrono::high_resolution_clock::now();
        // exp_recorder.sfc_cal_time += chrono::duration_cast<chrono::nanoseconds>(finish_sfc - start_sfc).count();
        auto start = chrono::high_resolution_clock::now();
        // predicted_index = predict(exp_recorder, query_point, leaf_node_num);
        if (is_reused)
        {
            float x1 = (query_point.x - x_0) * x_scale + x_0;
            float x2 = (query_point.y - y_0) * y_scale + y_0;
            // predicted_index = (int)(net->predict(query_point, x_scale, y_scale, x_0, y_0) * width);
            // predicted_index = net->predict(query_point, x_scale, y_scale, x_0, y_0) * width;
            predicted_index = net->predict(x1, x2) * leaf_node_num;
        }
        else
        {
            // predicted_index = (int)(net->predict(query_point) * width);
            predicted_index = net->predict(query_point) * leaf_node_num;
        }

        auto finish = chrono::high_resolution_clock::now();
        // cout << "leaf model predict Time: " << chrono::duration_cast<chrono::nanoseconds>(finish - start).count() << " net width: " << net->width << endl;
        exp_recorder.prediction_time += chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
        // net->print_parameters();
        auto start1 = chrono::high_resolution_clock::now();

        predicted_index = predicted_index < 0 ? 0 : predicted_index;
        predicted_index = predicted_index >= leaf_node_num ? leaf_node_num - 1 : predicted_index;
        LeafNode leafnode = leafnodes[predicted_index];

        if (leafnode.mbr.contains(query_point))
        {
            exp_recorder.page_access += 1;
            exp_recorder.search_length += 1;
            vector<Point>::iterator iter = find(leafnode.children->begin(), leafnode.children->end(), query_point);
            if (iter != leafnode.children->end())
            {
                // cout<< "find it" << endl;
                auto finish1 = chrono::high_resolution_clock::now();
                exp_recorder.search_time += chrono::duration_cast<chrono::nanoseconds>(finish1 - start1).count();
                return true;
            }
        }
        // predicted result is not correct
        int front = predicted_index + min_error;
        front = front < 0 ? 0 : front;
        int back = predicted_index + max_error;
        back = back >= leafnodes.size() ? leafnodes.size() - 1 : back;


        int gap = 1;
        int predicted_index_left = predicted_index - gap;
        int predicted_index_right = predicted_index + gap;
        while (predicted_index_left >= front && predicted_index_right <= back)
        {
            // search left
            exp_recorder.search_length += 1;
            LeafNode leafnode = leafnodes[predicted_index_left];
            if (leafnode.mbr.contains(query_point))
            {
                exp_recorder.page_access += 1;
                for (Point point : (*leafnode.children))
                {
                    if (query_point.x == point.x && query_point.y == point.y)
                    {
                        // cout<< "find it" << endl;
                        auto finish1 = chrono::high_resolution_clock::now();
                        exp_recorder.search_time += chrono::duration_cast<chrono::nanoseconds>(finish1 - start1).count();
                        return true;
                    }
                }
            }

            // search right
            exp_recorder.search_length += 1;
            leafnode = leafnodes[predicted_index_right];
            if (leafnode.mbr.contains(query_point))
            {
                exp_recorder.page_access += 1;
                for (Point point : (*leafnode.children))
                {
                    if (query_point.x == point.x && query_point.y == point.y)
                    {
                        // cout<< "find it" << endl;
                        auto finish1 = chrono::high_resolution_clock::now();
                        exp_recorder.search_time += chrono::duration_cast<chrono::nanoseconds>(finish1 - start1).count();
                        return true;
                    }
                }
            }
            gap++;
            predicted_index_left = predicted_index - gap;
            predicted_index_right = predicted_index + gap;
        }

        while (predicted_index_left >= front)
        {
            LeafNode leafnode = leafnodes[predicted_index_left];
            exp_recorder.search_length += 1;
            if (leafnode.mbr.contains(query_point))
            {
                exp_recorder.page_access += 1;
                for (Point point : (*leafnode.children))
                {
                    if (query_point.x == point.x && query_point.y == point.y)
                    {
                        // cout<< "find it" << endl;
                        auto finish1 = chrono::high_resolution_clock::now();
                        exp_recorder.search_time += chrono::duration_cast<chrono::nanoseconds>(finish1 - start1).count();
                        return true;
                    }
                }
            }
            gap++;
            predicted_index_left = predicted_index - gap;
        }

        while (predicted_index_right <= back)
        {
            exp_recorder.search_length += 1;
            LeafNode leafnode = leafnodes[predicted_index_right];

            if (leafnode.mbr.contains(query_point))
            {
                exp_recorder.page_access += 1;
                for (Point point : (*leafnode.children))
                {
                    if (query_point.x == point.x && query_point.y == point.y)
                    {
                        // cout<< "find it" << endl;
                        auto finish1 = chrono::high_resolution_clock::now();
                        exp_recorder.search_time += chrono::duration_cast<chrono::nanoseconds>(finish1 - start1).count();
                        return true;
                    }
                }
            }
            gap++;
            predicted_index_right = predicted_index + gap;
        }

        exp_recorder.point_not_found++;
        return false;
    }
    else
    {
        int predicted_index;
        // int predicted_index = net->predict(query_point) * width;
        auto start = chrono::high_resolution_clock::now();
        // if (is_reused)
        // {
        //     predicted_index = (int)(net->predict(query_point, x_gap, y_gap, x_0, y_0) * width);
        // }
        // else
        // {
        //     predicted_index = (int)(net->predict_(query_point) * width);
        // }

        // predicted_index = predict(exp_recorder, query_point, width);

        if (is_reused)
        {
            // predicted_index = (int)(net->predict(query_point, x_scale, y_scale, x_0, y_0) * width);
            // Point point((query_point.x - x_0) * x_scale + x_0, (query_point.y - y_0) * y_scale + y_0);
            // predicted_index = net->predict(query_point, x_scale, y_scale, x_0, y_0) * width;
            // predicted_index = net->predict(point) * width;

            float x1 = (query_point.x - x_0) * x_scale + x_0;
            float x2 = (query_point.y - y_0) * y_scale + y_0;
            // predicted_index = (int)(net->predict(query_point, x_scale, y_scale, x_0, y_0) * width);
            // predicted_index = net->predict(query_point, x_scale, y_scale, x_0, y_0) * width;
            predicted_index = net->predict(x1, x2) * width;
        }
        else
        {
            // predicted_index = (int)(net->predict(query_point) * width);
            predicted_index = net->predict(query_point) * width;
        }

        auto finish = chrono::high_resolution_clock::now();
        // cout << "Nonleaf model predict Time: " << chrono::duration_cast<chrono::nanoseconds>(finish - start).count() << " net width: " << net->width << endl;
        exp_recorder.prediction_time += chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
        // net->print_parameters();
        predicted_index = predicted_index < 0 ? 0 : predicted_index;
        predicted_index = predicted_index >= width ? width - 1 : predicted_index;
        // if (children.count(predicted_index) == 0)
        // {
        //     cout << "not find" << endl;
        //     return false;
        // }
        return children[predicted_index].point_query(exp_recorder, query_point);
    }
}

int RSMI::predict(ExpRecorder &exp_recorder, Point query_point, int width)
{
    // int predicted_index;

    if (is_reused)
    {
        // predicted_index = (int)(net->predict(query_point, x_scale, y_scale, x_0, y_0) * width);
        float x1 = (query_point.x - x_0) * x_scale + x_0;
        float x2 = (query_point.y - y_0) * y_scale + y_0;
        return (net->predict(x1, x2) * width);
    }
    else
    {
        // predicted_index = (int)(net->predict(query_point) * width);
        return (net->predict(query_point) * width);
    }

    // return predicted_index;
}

void RSMI::point_query(ExpRecorder &exp_recorder, vector<Point> &query_points)
{
    long size = query_points.size();
    // size = 100;
    // for (long i = 19890; i <= 19890; i++)
    for (long i = 0; i < size; ++i)
    {
        // cout << "i: " << i << endl;
        auto start = chrono::high_resolution_clock::now();
        point_query(exp_recorder, query_points[i]);
        auto finish = chrono::high_resolution_clock::now();
        exp_recorder.time += chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
    }
    exp_recorder.time /= size;
    exp_recorder.page_access /= size;
    exp_recorder.search_time /= size;
    exp_recorder.prediction_time /= size;
    exp_recorder.sfc_cal_time /= size;
    exp_recorder.search_steps /= size;
    exp_recorder.search_length /= size;
}

void RSMI::window_query(ExpRecorder &exp_recorder, vector<Mbr> query_windows)
{
    exp_recorder.is_window = true;
    long long time_cost = 0;
    int length = query_windows.size();
    double average_num_before_refine = 0;
    double avg_num_leaf_reached = 0; 
    double avg_num_leaf_skiped = 0;
    for (int i = 0; i < length; i++)
    {
        vector<Point> vertexes = query_windows[i].get_corner_points();
        auto start = chrono::high_resolution_clock::now();
        // vector<Point> window_query_results = window_query(exp_recorder, vertexes, query_windows[i]);
        window_query(exp_recorder, vertexes, query_windows[i]);
        auto finish = chrono::high_resolution_clock::now();
        std:: cout << "num record before refine" << exp_recorder.num_of_rec_before_refine << endl;
         std::cout << "num record after refine"  << exp_recorder.window_query_results.size()<< endl; 
         std::cout << "total num of leaf reached" << exp_recorder.total_num_of_leaf_reached << endl;
         std::cout << "num of leaf skipped" << exp_recorder.num_of_leaf_skipped << endl;
        exp_recorder.window_query_result_size += exp_recorder.window_query_results.size();
        exp_recorder.window_query_results.clear();
        exp_recorder.window_query_results.shrink_to_fit();
        exp_recorder.time += chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
        average_num_before_refine += exp_recorder.num_of_rec_before_refine ;
        avg_num_leaf_reached += exp_recorder.total_num_of_leaf_reached;
        avg_num_leaf_skiped += exp_recorder.num_of_leaf_skipped;
        exp_recorder.num_of_rec_before_refine = 0;
        exp_recorder.total_num_of_leaf_reached =0 ;
        exp_recorder.num_of_leaf_skipped = 0 ;
    
    }
    // cout << "exp_recorder.window_query_result_size: " << exp_recorder.window_query_result_size << endl;
    exp_recorder.time /= length;
    exp_recorder.page_access = (double)exp_recorder.page_access / length;
    exp_recorder.prediction_time /= length;
    exp_recorder.search_time /= length;
    exp_recorder.search_length /= length;
    cout<< "average num before refine" << (average_num_before_refine/= length) << endl;
    cout << "average num of leaf reached" << (avg_num_leaf_reached/= length) << endl;
    cout << "average num of leaf skipped" << (avg_num_leaf_skiped /= length) << endl;
}

void RSMI::window_query(ExpRecorder &exp_recorder, vector<Point> vertexes, Mbr query_window)
{ 
    // vector<Point> window_query_results;
    if (is_last)
    {
        auto start = chrono::high_resolution_clock::now();
        int leafnodes_size = leafnodes.size();
        if (leafnodes_size == 0)
        {
            return;
        }
        int front = leafnodes_size - 1;
        int back = 0;
        if (leaf_node_num == 0)
        {
            return;
        }
        else if (leaf_node_num < 2)
        {
            front = 0;
            back = 0;
        }
        else
        {
            int max = 0;
            int min = width;
            for (size_t i = 0; i < vertexes.size(); i++)
            {
                int predicted_index = predict(exp_recorder, vertexes[i], leaf_node_num);
                // if (predicted_index < 0)
                // {
                //     predicted_index = 0;
                // }
                // if (predicted_index > width)
                // {
                //     predicted_index = width;
                // }
                // predicted_index = predicted_index < 0 ? 0 : predicted_index;
                // predicted_index = predicted_index > width ? width : predicted_index;
                int predicted_index_max = predicted_index + max_error;
                int predicted_index_min = predicted_index + min_error;
                if (predicted_index_min < min)
                {
                    min = predicted_index_min;
                }
                if (predicted_index_max > max)
                {
                    max = predicted_index_max;
                }
            }
            front = min < 0 ? 0 : min;
            back = max >= leafnodes_size ? leafnodes_size - 1 : max;
            if (back < front)
            {
                return;
            }
            // std::cout << "min: " << min << std::endl;
            // std::cout << "max: " << max << std::endl;
            // std::cout << "front: " << front << std::endl;
            // std::cout << "back: " << back << std::endl;
            // front = min < 0 ? 0 : min;
            // back = max >= leafnodes_size ? leafnodes_size - 1 : max;
        }
        auto finish = chrono::high_resolution_clock::now();
        exp_recorder.prediction_time += chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
        start = chrono::high_resolution_clock::now();
        for (size_t i = front; i <= back; i++)
        {
            LeafNode leafnode = leafnodes[i];
            exp_recorder.total_num_of_leaf_reached +=1;
            if (leafnode.mbr.interact(query_window))
            {
                exp_recorder.page_access += 1;
                for (Point point : (*leafnode.children))
                {
                    num_rec_before_refine_leaf +=1;
                    if (query_window.contains(point))
                    {
                        exp_recorder.window_query_results.push_back(point);
                        // exp_recorder.window_query_result_size++;
                    }
                }
            }else{
                exp_recorder.num_of_leaf_skipped +=1;
            }
        }
        finish = chrono::high_resolution_clock::now();
        exp_recorder.num_of_rec_before_refine += num_rec_before_refine_leaf;
        exp_recorder.search_time += chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
        exp_recorder.search_length += (back - front + 1);

        // return window_query_results;
        return;
    }
    else
    {
        int front = width;
        int back = 0;
        for (size_t i = 0; i < vertexes.size(); i++)
        {
            int predicted_index = predict(exp_recorder, vertexes[i], width);
            predicted_index = predicted_index < 0 ? 0 : predicted_index;
            predicted_index = predicted_index >= width ? width - 1 : predicted_index;

            // if (predicted_index < 0)
            // {
            //     predicted_index = 0;
            // }
            // if (predicted_index >= width)
            // {
            //     predicted_index = width - 1;
            // }

            if (predicted_index < front)
            {
                front = predicted_index;
            }
            if (predicted_index > back)
            {
                back = predicted_index;
            }
        }
        for (size_t i = front; i <= back; i++)
        {
            if (children.count(i) == 0)
            {
                continue;
            }
            if (children[i].mbr.interact(query_window))
            {

                children[i].window_query(exp_recorder, vertexes, query_window);
                // vector<Point> temp_result = children[i].window_query(exp_recorder, vertexes, query_window);
                // window_query_results.insert(window_query_results.end(), temp_result.begin(), temp_result.end());
            }
        }

    }
    // return window_query_results;
    
}

// this method is for knn query
void RSMI::window_query(ExpRecorder &exp_recorder, vector<Point> vertexes, Mbr query_window, float boundary, int k, Point query_point, float &kth)
{
    if (is_last)
    {
        int leafnodes_size = leafnodes.size();
        int front = leafnodes_size - 1;
        int back = 0;
        if (leaf_node_num == 0)
        {
            return;
        }
        else if (leaf_node_num < 2)
        {
            front = 0;
            back = 0;
        }
        else
        {
            auto start = chrono::high_resolution_clock::now();
            int max = 0;
            int min = width;
            for (size_t i = 0; i < vertexes.size(); i++)
            {
                // auto start = chrono::high_resolution_clock::now();
                int predicted_index = predict(exp_recorder, vertexes[i], leaf_node_num);
                // auto finish = chrono::high_resolution_clock::now();
                predicted_index = predicted_index < 0 ? 0 : predicted_index;
                predicted_index = predicted_index > width ? width : predicted_index;
                int predicted_index_max = predicted_index + max_error;
                int predicted_index_min = predicted_index + min_error;
                if (predicted_index_min < min)
                {
                    min = predicted_index_min;
                }
                if (predicted_index_max > max)
                {
                    max = predicted_index_max;
                }
            }

            front = min < 0 ? 0 : min;
            back = max >= leafnodes_size ? leafnodes_size - 1 : max;
            auto finish = chrono::high_resolution_clock::now();
            exp_recorder.prediction_time += chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
        }
        auto start = chrono::high_resolution_clock::now();

        for (size_t i = front; i <= back; i++)
        {
            LeafNode leafnode = leafnodes[i];
            float dis = leafnode.mbr.cal_dist(query_point);
            if (dis > boundary)
            {
                continue;
            }
            // if (exp_recorder.pq.size() >= k && dis > kth)
            // {
            //     continue;
            // }
            if (leafnode.mbr.interact(query_window))
            {
                exp_recorder.page_access += 1;
                for (Point point : (*leafnode.children))
                {
                    if (query_window.contains(point))
                    {
                        if (point.cal_dist(query_point) <= boundary)
                        {
                            exp_recorder.pq.push(point);
                            // if (exp_recorder.pq.size() >= k && kth < point.temp_dist)
                            // {
                            //     kth = point.temp_dist;
                            // }
                        }
                    }
                }
            }
        }
        exp_recorder.search_length += (back - front + 1);
        auto finish = chrono::high_resolution_clock::now();
        exp_recorder.search_time += chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
        return;
    }
    else
    {
        auto start = chrono::high_resolution_clock::now();

        int front = width;
        int back = 0;
        for (size_t i = 0; i < vertexes.size(); i++)
        {
            // auto start = chrono::high_resolution_clock::now();
            int predicted_index = predict(exp_recorder, vertexes[i], width);
            // auto finish = chrono::high_resolution_clock::now();
            predicted_index = predicted_index < 0 ? 0 : predicted_index;
            predicted_index = predicted_index >= width ? width - 1 : predicted_index;
            if (predicted_index < front)
            {
                front = predicted_index;
            }
            if (predicted_index > back)
            {
                back = predicted_index;
            }
        }
        auto finish = chrono::high_resolution_clock::now();
        exp_recorder.prediction_time += chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
        for (size_t i = front; i <= back; i++)
        {
            if (children.count(i) == 0)
            {
                continue;
            }
            if (exp_recorder.pq.size() >= k && children[i].mbr.cal_dist(query_point) > kth)
            {
                continue;
            }
            if (children[i].mbr.interact(query_window))
            {
                children[i].window_query(exp_recorder, vertexes, query_window, boundary, k, query_point, kth);
            }
        }
    }
}

vector<Point> RSMI::acc_window_query(ExpRecorder &exp_recorder, Mbr query_window)
{
    vector<Point> window_query_results;
    if (is_last)
    {
        for (LeafNode leafnode : leafnodes)
        {
            if (leafnode.mbr.interact(query_window))
            {
                exp_recorder.page_access += 1;
                for (Point point : (*leafnode.children))
                {
                    if (query_window.contains(point))
                    {
                        window_query_results.push_back(point);
                    }
                }
            }
        }
    }
    else
    {
        map<int, RSMI>::iterator iter = children.begin();
        while (iter != children.end())
        {
            if (iter->second.mbr.interact(query_window))
            {
                vector<Point> temp_result = iter->second.acc_window_query(exp_recorder, query_window);
                window_query_results.insert(window_query_results.end(), temp_result.begin(), temp_result.end());
            }
            iter++;
        }
    }
    return window_query_results;
}

void RSMI::acc_window_query(ExpRecorder &exp_recorder, vector<Mbr> query_windows)
{
    int length = query_windows.size();
    for (int i = 0; i < length; i++)
    {
        auto start = chrono::high_resolution_clock::now();
        exp_recorder.acc_window_query_result_size += acc_window_query(exp_recorder, query_windows[i]).size();
        auto finish = chrono::high_resolution_clock::now();
        exp_recorder.time += chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
    }
    exp_recorder.time = exp_recorder.time / length;
    exp_recorder.page_access = (double)exp_recorder.page_access / length;
}

double RSMI::cal_rho(Point point)
{
    // return 1;
    int predicted_index = net->predict(point) * width;
    predicted_index = predicted_index < 0 ? 0 : predicted_index;
    predicted_index = predicted_index >= width ? width - 1 : predicted_index;
    long long bk = 0;
    for (int i = 0; i < predicted_index; i++)
    {
        if (children[i].N == 0)
        {
            return 1;
        }
        bk += children[i].N;
    }
    if (children[predicted_index].N == 0)
    {
        return 1;
    }
    long long bk1 = bk + children[predicted_index].N;
    double result = bk1 * 1.0 / bk;
    result = result > 1 ? result : 1;
    result = result > 4 ? 4 : result;
    return result;
}

void RSMI::kNN_query(ExpRecorder &exp_recorder, vector<Point> query_points, int k)
{
    exp_recorder.is_knn = true;
    cout << "RSMI::kNN_query" << endl;
    int length = query_points.size();
    exp_recorder.time = 0;
    exp_recorder.page_access = 0;
    // length = 2;
    for (int i = 0; i < length; i++)
    {
        priority_queue<Point, vector<Point>, sortForKNN2> temp_pq;
        exp_recorder.pq = temp_pq;
        auto start = chrono::high_resolution_clock::now();
        kNN_query(exp_recorder, query_points[i], k);
        auto finish = chrono::high_resolution_clock::now();
        vector<Point> knn_result;
        while (!exp_recorder.pq.empty())
        {
            knn_result.push_back(exp_recorder.pq.top());
            // std::cout << "point_pq.top().temp_dist: " << point_pq.top().temp_dist << std::endl;
            exp_recorder.pq.pop();
        }
        exp_recorder.window_query_results.clear();
        exp_recorder.window_query_results.shrink_to_fit();
        long long temp_time = chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
        exp_recorder.time += temp_time;
        exp_recorder.knn_query_results.insert(exp_recorder.knn_query_results.end(), knn_result.begin(), knn_result.end());
    }
    exp_recorder.time /= length;
    exp_recorder.page_access = (double)exp_recorder.page_access / length;
    exp_recorder.k_num = k;
    exp_recorder.prediction_time /= length;
    exp_recorder.search_time /= length;
    exp_recorder.knn_r_enlarged_num /= length;
    exp_recorder.search_length /= length;
}

void RSMI::kNN_query(ExpRecorder &exp_recorder, Point query_point, int k)
{
    // vector<Point> result;
    double rh0 = 1.0;
    // double rh0 = cal_rho(query_point);
    // rh0 = 0.5;
    // cout<< "rh0: " << rh0 << endl;
    float knnquery_side = sqrt((float)k / N) * rh0;

    // priority_queue<NodeExtend *, vector<NodeExtend *>, sortPQ_Desc> point_pq;
    // priority_queue<Point, vector<Point>, sortForKNN2> point_pq;
    while (true)
    {
        Mbr mbr = Mbr::get_mbr(query_point, knnquery_side);
        vector<Point> vertexes = mbr.get_corner_points();
        // vector<Point> temp_result = window_query(exp_recorder, vertexes, mbr);
        window_query(exp_recorder, vertexes, mbr);
        vector<Point> temp_result = exp_recorder.window_query_results;
        if (temp_result.size() >= k)
        {
            // sort(temp_result.begin(), temp_result.end(), sort_for_kNN(query_point));
            // Point last = temp_result[k - 1];
            double dist_furthest = 0;
            int dist_furthest_i = 0;
            for (size_t i = 0; i < temp_result.size(); i++)
            {
                // Point temp_point = temp_result[i];
                double temp_dist = temp_result[i].cal_dist(query_point);
                // std::cout << "temp_dist: " << temp_dist << std::endl;

                temp_result[i].temp_dist = temp_dist;
                if (exp_recorder.pq.size() < k)
                {
                    // NodeExtend *temp = new NodeExtend(temp_result[i], temp_dist);
                    exp_recorder.pq.push(temp_result[i]);
                }
                else
                {
                    if (exp_recorder.pq.top().temp_dist < temp_dist)
                    {
                        continue;
                    }
                    else
                    {
                        // NodeExtend *temp = new NodeExtend(temp_result[i], temp_dist);
                        exp_recorder.pq.pop();
                        exp_recorder.pq.push(temp_result[i]);
                    }
                }
            }

            // if (last.cal_dist(query_point) <= knnquery_side)
            if (exp_recorder.pq.top().temp_dist <= knnquery_side)
            {
                break;
            }
        }
        exp_recorder.knn_r_enlarged_num++;
        knnquery_side *= 2;
    }
    // return result;
}

void RSMI::acc_kNN_query(ExpRecorder &exp_recorder, vector<Point> query_points, int k)
{
    int length = query_points.size();
    // length = 1;
    for (int i = 0; i < length; i++)
    {
        auto start = chrono::high_resolution_clock::now();
        vector<Point> knn_result = acc_kNN_query(exp_recorder, query_points[i], k);
        // for (size_t j = 0; j < k; j++)
        // {
        //     cout << "dist: " << query_points[i].cal_dist(knn_result[j]) << endl;
        // }
        auto finish = chrono::high_resolution_clock::now();
        exp_recorder.time += chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
        exp_recorder.acc_knn_query_results.insert(exp_recorder.acc_knn_query_results.end(), knn_result.begin(), knn_result.end());
    }
    exp_recorder.time /= length;
    exp_recorder.k_num = k;
    exp_recorder.page_access = (double)exp_recorder.page_access / length;
}

vector<Point> RSMI::acc_kNN_query(ExpRecorder &exp_recorder, Point query_point, int k)
{
    vector<Point> result;
    float knnquery_side = sqrt((float)k / N);
    while (true)
    {
        Mbr mbr = Mbr::get_mbr(query_point, knnquery_side);
        vector<Point> temp_result = acc_window_query(exp_recorder, mbr);
        if (temp_result.size() >= k)
        {
            sort(temp_result.begin(), temp_result.end(), sort_for_kNN(query_point));
            Point last = temp_result[k - 1];
            if (last.cal_dist(query_point) <= knnquery_side)
            {
                auto bn = temp_result.begin();
                auto en = temp_result.begin() + k;
                vector<Point> vec(bn, en);
                result = vec;
                break;
            }
        }
        knnquery_side = knnquery_side * 2;
    }
    return result;
}

void RSMI::insert(ExpRecorder &exp_recorder, Point point)
{
    auto start = chrono::high_resolution_clock::now();
    exp_recorder.update_num++;
    int predicted_index = predict(exp_recorder, point, width);
    predicted_index = predicted_index < 0 ? 0 : predicted_index;
    predicted_index = predicted_index >= width ? width - 1 : predicted_index;
    auto finish = chrono::high_resolution_clock::now();
    exp_recorder.prediction_time += chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
    if (is_last)
    {
        if (N == Constants::THRESHOLD)
        {
            // cout << "rebuild: " << endl;
            is_last = false;
            vector<Point> points;
            auto start = chrono::high_resolution_clock::now();
            for (LeafNode leafNode : leafnodes)
            {
                points.insert(points.end(), leafNode.children->begin(), leafNode.children->end());
            }
            points.push_back(point);
            if (level == exp_recorder.depth)
            {
                exp_recorder.new_depth = level + 1;
            }
            build(exp_recorder, points);

            // rebuild(exp_recorder, points);
            auto finish = chrono::high_resolution_clock::now();
            exp_recorder.rebuild_time += chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
            exp_recorder.rebuild_num++;
        }
        else
        {
            int insertedIndex = predicted_index / Constants::PAGESIZE;
            LeafNode leafnode = leafnodes[insertedIndex];
            // if (leafnode.is_full())
            // {
            //     LeafNode right = leafnode.split1();
            //     leafnodes.insert(leafnodes.begin() + insertedIndex + 1, right);
            //     leaf_node_num++;
            // }
            // leafnode.add_point(point);

            if (leafnode.is_full())
            {
                leafnode.add_point(point);
                sort(leafnode.children->begin(), leafnode.children->end(), sort_curve_val());
                LeafNode right = leafnode.split1();
                leafnodes.insert(leafnodes.begin() + insertedIndex + 1, right);
                min_error--;
                max_error++;
                // cout << "creat new page" << endl;
            }
            else
            {
                leafnode.add_point(point);
                sort(leafnode.children->begin(), leafnode.children->end(), sort_curve_val());
            }

            N++;
            width++;
        }
    }
    else
    {
        if (children.count(predicted_index) == 0)
        {
            return;
        }
        if (level == 0)
        {
            exp_recorder.changing_histogram.update(predicted_index);
        }
        children[predicted_index].insert(exp_recorder, point);
    }
}

bool RSMI::insert(ExpRecorder &exp_recorder, vector<Point> &points)
{
    auto start = chrono::high_resolution_clock::now();
    for (int i = 0; i < points.size(); i++)
    {
        insert(exp_recorder, points[i]);
    }
    exp_recorder.previous_insert_num += points.size();
    auto finish = chrono::high_resolution_clock::now();
    bool is_rebuild = rebuild_index::is_rebuild(exp_recorder, "Z");
    long long previous_time = exp_recorder.insert_time * exp_recorder.previous_insert_num;
    exp_recorder.insert_time = (previous_time + chrono::duration_cast<chrono::nanoseconds>(finish - start).count()) / exp_recorder.previous_insert_num;
    return is_rebuild;
}

void RSMI::insert(ExpRecorder &exp_recorder, int index)
{
    string dataset = Constants::QUERYPROFILES + "update/skewed_128000000_4_12800000_" + to_string(index) + "_.csv";
    FileReader filereader(dataset, ",");
    vector<Point> points = filereader.get_points();
    auto start = chrono::high_resolution_clock::now();
    for (int i = 0; i < points.size(); i++)
    {
        insert(exp_recorder, points[i]);
    }
    auto finish = chrono::high_resolution_clock::now();
    long long previous_time = exp_recorder.insert_time * exp_recorder.previous_insert_num;
    exp_recorder.previous_insert_num += points.size();
    exp_recorder.insert_time = (previous_time + chrono::duration_cast<chrono::nanoseconds>(finish - start).count()) / exp_recorder.previous_insert_num;
}

void RSMI::insert(ExpRecorder &exp_recorder)
{
    vector<Point> points = Point::get_inserted_points(exp_recorder.insert_num, exp_recorder.insert_points_distribution);
    auto start = chrono::high_resolution_clock::now();
    for (int i = 0; i < points.size(); i++)
    {
        insert(exp_recorder, points[i]);
    }
    auto finish = chrono::high_resolution_clock::now();
    long long previous_time = exp_recorder.insert_time * exp_recorder.previous_insert_num;
    exp_recorder.previous_insert_num += points.size();
    exp_recorder.insert_time = (previous_time + chrono::duration_cast<chrono::nanoseconds>(finish - start).count()) / exp_recorder.previous_insert_num;
}

void RSMI::remove(ExpRecorder &exp_recorder, Point point)
{
    int predicted_index = net->predict(point) * width;
    predicted_index = predicted_index < 0 ? 0 : predicted_index;
    predicted_index = predicted_index >= N ? N - 1 : predicted_index;
    if (is_last)
    {
        // cout << "predicted_index: " << predicted_index << endl;
        int front = predicted_index + min_error;
        front = front < 0 ? 0 : front;
        int back = predicted_index + max_error;
        back = back >= N ? N - 1 : back;
        front = front / Constants::PAGESIZE;
        back = back / Constants::PAGESIZE;
        for (size_t i = front; i <= back; i++)
        {
            LeafNode leafnode = leafnodes[i];
            vector<Point>::iterator iter = find(leafnode.children->begin(), leafnode.children->end(), point);
            if (leafnode.mbr.contains(point) && leafnode.delete_point(point))
            {
                N--;
                break;
            }
        }
    }
    else
    {
        if (children.count(predicted_index) == 0)
        {
            return;
        }
        children[predicted_index].remove(exp_recorder, point);
    }
}

void RSMI::remove(ExpRecorder &exp_recorder, vector<Point> points)
{
    auto start = chrono::high_resolution_clock::now();
    for (Point point : points)
    {
        remove(exp_recorder, point);
    }
    auto finish = chrono::high_resolution_clock::now();
    long long oldTimeCost = exp_recorder.delete_time * exp_recorder.delete_num;
    exp_recorder.delete_num += points.size();
    exp_recorder.delete_time = (oldTimeCost + oldTimeCost + chrono::duration_cast<chrono::nanoseconds>(finish - start).count()) / exp_recorder.delete_num;
}

void RSMI::traverse(ExpRecorder &exp_recorder)
{
    auto start = chrono::high_resolution_clock::now();

    if (is_last)
    {
    }
    else
    {
        map<int, RSMI>::iterator iter = children.begin();
        while (iter != children.end())
        {
            iter->second.traverse(exp_recorder);
            iter++;
        }
    }

    auto finish = chrono::high_resolution_clock::now();
    exp_recorder.traverse_time = chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
}

void RSMI::clear(ExpRecorder &exp_recorder)
{
    level = 0;
    index = 0;
    N = 0;
    max_error = 0;
    min_error = 0;
    width = 0;
    leaf_node_num = 0;
    is_last = false;

    x_gap = 1.0;
    x_scale = 1.0;
    x_0 = 0;
    x_1 = 0;
    y_gap = 1.0;
    y_scale = 1.0;
    y_0 = 0;
    y_1 = 0;
    is_reused = false;
    side = 0;

    children.clear();
    leafnodes.clear();
    leafnodes.shrink_to_fit();
    points_x.clear();
    points_x.shrink_to_fit();
    points_y.clear();
    points_y.shrink_to_fit();
    sampling_rate = 1.0;
    // exp_recorder.previous_insert_num = 0;
}