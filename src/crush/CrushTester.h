// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#ifndef CEPH_CRUSH_TESTER_H
#define CEPH_CRUSH_TESTER_H

#include "crush/CrushWrapper.h"

#include <fstream>
#include <sstream>

class CrushTester {
  CrushWrapper& crush;
  ostream& err;
  int verbose;

  map<int, int> device_weight;
  int min_rule, max_rule;
  int min_x, max_x;
  int min_rep, max_rep;

  int num_batches;
  bool use_crush;

  float mark_down_device_ratio;
  float mark_down_bucket_ratio;

  bool output_utilization;
  bool output_utilization_all;
  bool output_statistics;
  bool output_bad_mappings;
  bool output_choose_tries;

  bool output_data_file;
  bool output_csv;

  string output_data_file_name;


  void adjust_weights(vector<__u32>& weight);
  int get_maximum_affected_by_rule(int ruleno);
  map<int,int> get_collapsed_mapping();
  bool check_valid_placement(int ruleno, vector<int> out, const vector<__u32>& weight);
  int random_placement(int ruleno, vector<int>& out, int maxout, vector<__u32>& weight);

  // scaffolding to store data for off-line processing
   struct tester_data_set {
     vector <string> device_utilization;
     vector <string> device_utilization_all;
     vector <string> placement_information;
     vector <string> batch_device_utilization_all;
     vector <string> batch_device_expected_utilization_all;
     map<int, float> proportional_weights;
     map<int, float> proportional_weights_all;
     map<int, float> absolute_weights;
   } ;

   void write_to_csv(ofstream& csv_file, vector<string>& payload)
   {
     if (csv_file.good())
       for (vector<string>::iterator it = payload.begin(); it != payload.end(); it++)
         csv_file << (*it);
   }

   void write_to_csv(ofstream& csv_file, map<int, float>& payload)
   {
     if (csv_file.good())
       for (map<int, float>::iterator it = payload.begin(); it != payload.end(); it++)
         csv_file << (*it).first << ',' << (*it).second << std::endl;
   }

   void write_data_set_to_csv(string user_tag, tester_data_set& tester_data)
   {

     ofstream device_utilization_file ((user_tag + (string)"-device_utilization.csv").c_str());
     ofstream device_utilization_all_file ((user_tag + (string)"-device_utilization_all.csv").c_str());
     ofstream placement_information_file ((user_tag + (string)"-placement_information.csv").c_str());
     ofstream batch_device_utilization_all_file ((user_tag + (string)"-batch_device_utilization_all.csv").c_str());
     ofstream batch_device_expected_utilization_all_file ((user_tag + (string)"-batch_device_expected_utilization_all.csv").c_str());
     ofstream proportional_weights_file ((user_tag + (string)"-proportional_weights.csv").c_str());
     ofstream proportional_weights_all_file ((user_tag + (string)"-proportional_weights_all.csv").c_str());
     ofstream absolute_weights_file ((user_tag + (string)"-absolute_weights.csv").c_str());

     write_to_csv(device_utilization_file, tester_data.device_utilization);
     write_to_csv(device_utilization_all_file, tester_data.device_utilization_all);
     write_to_csv(placement_information_file, tester_data.placement_information);
     write_to_csv(batch_device_utilization_all_file, tester_data.batch_device_utilization_all);
     write_to_csv(batch_device_expected_utilization_all_file, tester_data.batch_device_expected_utilization_all);
     write_to_csv(proportional_weights_file, tester_data.proportional_weights);
     write_to_csv(proportional_weights_all_file, tester_data.proportional_weights_all);
     write_to_csv(absolute_weights_file, tester_data.absolute_weights);

     device_utilization_file.close();
     device_utilization_all_file.close();
     placement_information_file.close();
     batch_device_expected_utilization_all_file.close();
     batch_device_utilization_all_file.close();
     proportional_weights_file.close();
     absolute_weights_file.close();
   }

   void write_integer_indexed_vector_data_string(vector<string> &dst, int index, vector<int> vector_data);
   void write_integer_indexed_vector_data_string(vector<string> &dst, int index, vector<float> vector_data);
   void write_integer_indexed_scalar_data_string(vector<string> &dst, int index, int scalar_data);
   void write_integer_indexed_scalar_data_string(vector<string> &dst, int index, float scalar_data);

public:
  CrushTester(CrushWrapper& c, ostream& eo, int verbosity=0)
    : crush(c), err(eo), verbose(verbosity),
      min_rule(-1), max_rule(-1),
      min_x(-1), max_x(-1),
      min_rep(-1), max_rep(-1),
      num_batches(1),
      use_crush(true),
      mark_down_device_ratio(0.0),
      mark_down_bucket_ratio(1.0),
      output_utilization(false),
      output_utilization_all(false),
      output_statistics(false),
      output_bad_mappings(false),
      output_choose_tries(false),
      output_data_file(false),
      output_csv(false),
      output_data_file_name("")

  { }

  void set_output_data_file_name(string name) {
    output_data_file_name = name;
  }
  void set_output_data_file(bool b) {
     output_data_file = b;
  }
  void set_output_csv(bool b) {
     output_csv = b;
  }
  void set_output_utilization(bool b) {
    output_utilization = b;
  }
  void set_output_utilization_all(bool b) {
    output_utilization_all = b;
  }
  void set_output_statistics(bool b) {
    output_statistics = b;
  }
  void set_output_bad_mappings(bool b) {
    output_bad_mappings = b;
  }
  void set_output_choose_tries(bool b) {
    output_choose_tries = b;
  }

  void set_batches(int b) {
    num_batches = b;
  }
  void set_random_placement() {
    use_crush = false;
  }
  void set_bucket_down_ratio(float bucket_ratio) {
    mark_down_bucket_ratio = bucket_ratio;
  }
  void set_device_down_ratio(float device_ratio) {
    mark_down_device_ratio = device_ratio;
  }
  void set_device_weight(int dev, float f);

  void set_min_rep(int r) {
    min_rep = r;
  }
  void set_max_rep(int r) {
    max_rep = r;
  }
  void set_num_rep(int r) {
    min_rep = max_rep = r;
  }
  
  void set_min_x(int x) {
    min_x = x;
  }
  void set_max_x(int x) {
    max_x = x;
  }
  void set_x(int x) {
    min_x = max_x = x;
  }

  void set_min_rule(int rule) {
    min_rule = rule;
  }
  void set_max_rule(int rule) {
    max_rule = rule;
  }
  void set_rule(int rule) {
    min_rule = max_rule = rule;
  }

  int test();
};

#endif
