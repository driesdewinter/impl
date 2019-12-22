#include <iostream>

static int next_doc_id = 0;

struct doc
{
  virtual ~doc() {}
  virtual void print(std::ostream& os) = 0;
};

struct jpg : doc
{
  jpg() : id(next_doc_id++) {}
  void print(std::ostream& os) override { os << "jpg" << id << "\n"; }
  int id;
};

struct pdf : doc
{
  pdf() : id(next_doc_id++) {}
  void print(std::ostream& os) override { os << "pdf" << id << "\n"; }
  int id;
};

void print_to_stdout(doc& d)
{
  d.print(std::cout);
}

int main()
{
  pdf p;
  print_to_stdout(p);
  jpg j;
  print_to_stdout(j);
  return 0;
}
