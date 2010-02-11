public class Employee
{
  public int id;
  public String firstName;
  public String lastName;
  public String position;
  public String department;
  public int salary;

  public Employee()
  {
  }

  public Employee(int id, String firstName, String lastName,
                  String position, String department, int salary)
  {
    this.id = id;
    this.firstName = firstName;
    this.lastName = lastName;
    this.position = position;
    this.department = department;
    this.salary = salary;
  }
}
